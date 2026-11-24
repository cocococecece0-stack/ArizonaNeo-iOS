#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, ANDTestStatus) {
    ANDTestStatusPass = 0,
    ANDTestStatusFail = 1,
    ANDTestStatusBlocked = 2
};

@interface ANDTestResult : NSObject

@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) ANDTestStatus status;
@property (nonatomic, copy) NSString *message;
@property (nonatomic, assign) double durationMs;

- (NSString *)statusString;
- (NSDictionary<NSString *, id> *)reportDictionary;

@end

@interface DeviceRuntimeTests : NSObject

@property (nonatomic, strong, readonly) NSArray<ANDTestResult *> *results;
@property (nonatomic, copy, readonly) NSString *overallStatus;
@property (nonatomic, copy, readonly) NSString *jitStatus;
@property (nonatomic, copy, readonly) NSString *ffiStatus;

- (void)runAllTests;

@end
