#import "ReportGenerator.h"

#include <sys/utsname.h>

static NSString * const ANDLuaJITCommit = @"96f7b297326e9fd454b7186c6858b2773f94a0dc";
static NSString * const ANDDeploymentTarget = @"15.0";

static NSString *ANDJSONEscape(NSString *input) {
    if (input.length == 0) {
        return @"";
    }
    NSMutableString *escaped = [NSMutableString stringWithCapacity:input.length];
    for (NSUInteger index = 0; index < input.length; index++) {
        unichar character = [input characterAtIndex:index];
        switch (character) {
            case '"': [escaped appendString:@"\\\""]; break;
            case '\\': [escaped appendString:@"\\\\"]; break;
            case '\n': [escaped appendString:@"\\n"]; break;
            case '\r': [escaped appendString:@"\\r"]; break;
            case '\t': [escaped appendString:@"\\t"]; break;
            default:
                if (character < 0x20) {
                    [escaped appendFormat:@"\\u%04x", character];
                } else {
                    [escaped appendFormat:@"%C", character];
                }
        }
    }
    return escaped;
}

static NSString *ANDJSONString(id value) {
    if (value == nil || [value isKindOfClass:[NSNull class]]) {
        return @"\"\"";
    }
    return [NSString stringWithFormat:@"\"%@\"", ANDJSONEscape([value description])];
}

static NSString *ANDMachineIdentifier(void) {
    struct utsname systemInfo;
    if (uname(&systemInfo) == 0) {
        return [NSString stringWithUTF8String:systemInfo.machine];
    }
    return @"unknown";
}

static NSString *ANDArchitecture(void) {
    struct utsname systemInfo;
    if (uname(&systemInfo) == 0) {
        return [NSString stringWithUTF8String:systemInfo.machine];
    }
    return @"arm64";
}

@implementation ReportGenerator

+ (NSURL *)writeReportWithTests:(NSArray<NSDictionary<NSString *, id> *> *)tests
                       overall:(NSString *)overall
                      jitStatus:(NSString *)jitStatus
                      ffiStatus:(NSString *)ffiStatus
                         error:(NSError **)error {
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                                     NSUserDomainMask, YES);
    if (paths.count == 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"ArizonaNeoDeviceTest"
                                        code:1
                                    userInfo:@{NSLocalizedDescriptionKey: @"Documents directory unavailable"}];
        }
        return nil;
    }
    NSString *directory = [paths[0] stringByAppendingPathComponent:@"ArizonaNeoDeviceTest"];
    NSFileManager *manager = [NSFileManager defaultManager];
    if (![manager createDirectoryAtPath:directory
            withIntermediateDirectories:YES
                             attributes:nil
                                  error:error]) {
        return nil;
    }

    NSDictionary *info = [NSBundle mainBundle].infoDictionary;
    NSString *appVersion = info[@"CFBundleShortVersionString"] ?: @"0.1";
    NSString *iosVersion = [UIDevice currentDevice].systemVersion ?: @"unknown";
    NSString *deviceModel = [UIDevice currentDevice].model ?: @"unknown";
    NSString *machine = ANDMachineIdentifier();

    NSISO8601DateFormatter *formatter = [NSISO8601DateFormatter new];
    formatter.formatOptions = NSISO8601DateFormatWithInternetDateTime;
    NSString *timestamp = [formatter stringFromDate:[NSDate date]];

    NSMutableString *json = [NSMutableString string];
    [json appendString:@"{\n"];
    [json appendFormat:@"  \"app_version\": %@,\n", ANDJSONString(appVersion)];
    [json appendFormat:@"  \"ios_version\": %@,\n", ANDJSONString(iosVersion)];
    [json appendFormat:@"  \"device_model\": %@,\n", ANDJSONString(deviceModel)];
    [json appendFormat:@"  \"device_machine\": %@,\n", ANDJSONString(machine)];
    [json appendFormat:@"  \"architecture\": %@,\n", ANDJSONString(ANDArchitecture())];
    [json appendFormat:@"  \"luajit_commit\": %@,\n", ANDJSONString(ANDLuaJITCommit)];
    [json appendFormat:@"  \"jit_status\": %@,\n", ANDJSONString(jitStatus)];
    [json appendFormat:@"  \"ffi_status\": %@,\n", ANDJSONString(ffiStatus)];
    [json appendFormat:@"  \"deployment_target\": %@,\n", ANDJSONString(ANDDeploymentTarget)];
    [json appendFormat:@"  \"timestamp\": %@,\n", ANDJSONString(timestamp)];
    [json appendFormat:@"  \"overall_status\": %@,\n", ANDJSONString(overall)];

    [json appendString:@"  \"tests\": [\n"];
    for (NSUInteger index = 0; index < tests.count; index++) {
        NSDictionary<NSString *, id> *test = tests[index];
        [json appendString:@"    {\n"];
        [json appendFormat:@"      \"name\": %@,\n", ANDJSONString(test[@"name"])];
        [json appendFormat:@"      \"status\": %@,\n", ANDJSONString(test[@"status"])];
        [json appendFormat:@"      \"message\": %@,\n", ANDJSONString(test[@"message"])];
        [json appendFormat:@"      \"duration_ms\": %@\n", test[@"duration_ms"] ?: @"0"];
        [json appendString:(index + 1 < tests.count) ? @"    },\n" : @"    }\n"];
    }
    [json appendString:@"  ],\n"];

    NSMutableArray<NSString *> *runtimeErrors = [NSMutableArray array];
    for (NSDictionary<NSString *, id> *test in tests) {
        if ([test[@"status"] isEqualToString:@"FAIL"] || [test[@"status"] isEqualToString:@"BLOCKED"]) {
            [runtimeErrors addObject:[NSString stringWithFormat:@"%@: %@",
                                      test[@"name"] ?: @"unknown",
                                      test[@"message"] ?: @"no message"]];
        }
    }
    [json appendString:@"  \"runtime_errors\": [\n"];
    for (NSUInteger index = 0; index < runtimeErrors.count; index++) {
        [json appendFormat:@"    %@%s\n", ANDJSONString(runtimeErrors[index]),
                           (index + 1 < runtimeErrors.count) ? "," : ""];
    }
    [json appendString:@"  ]\n"];
    [json appendString:@"}\n"];

    NSURL *url = [NSURL fileURLWithPath:[directory stringByAppendingPathComponent:@"report.json"]];
    NSData *data = [json dataUsingEncoding:NSUTF8StringEncoding];
    if (![data writeToURL:url options:NSDataWritingAtomic error:error]) {
        return nil;
    }
    return url;
}

@end
