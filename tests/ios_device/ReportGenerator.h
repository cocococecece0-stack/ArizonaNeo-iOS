#import <Foundation/Foundation.h>

@interface ReportGenerator : NSObject

+ (NSURL *)writeReportWithTests:(NSArray<NSDictionary<NSString *, id> *> *)tests
                       overall:(NSString *)overall
                      jitStatus:(NSString *)jitStatus
                      ffiStatus:(NSString *)ffiStatus
                         error:(NSError **)error;

@end
