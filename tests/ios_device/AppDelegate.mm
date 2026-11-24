#import "AppDelegate.h"

#import "DeviceRuntimeTests.h"
#import "ReportGenerator.h"

@interface ANDTestViewController : UIViewController

@property (nonatomic, strong) NSArray<ANDTestResult *> *results;
@property (nonatomic, copy) NSString *overallStatus;
@property (nonatomic, copy) NSString *jitStatus;
@property (nonatomic, copy) NSString *ffiStatus;
@property (nonatomic, strong) NSURL *reportURL;
@property (nonatomic, copy) NSString *reportError;

@end

@implementation ANDTestViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor systemBackgroundColor];

    UILabel *title = [UILabel new];
    title.text = @"ArizonaNeo Device Test";
    title.font = [UIFont boldSystemFontOfSize:22.0];
    title.textAlignment = NSTextAlignmentCenter;

    NSDictionary *info = [NSBundle mainBundle].infoDictionary;
    NSString *version = info[@"CFBundleShortVersionString"] ?: @"0.1";
    UILabel *versionLabel = [UILabel new];
    versionLabel.text = [NSString stringWithFormat:@"App version %@", version];
    versionLabel.font = [UIFont systemFontOfSize:13.0];
    versionLabel.textAlignment = NSTextAlignmentCenter;

    UILabel *summary = [UILabel new];
    summary.numberOfLines = 0;
    summary.font = [UIFont systemFontOfSize:14.0];
    summary.text = [NSString stringWithFormat:@"Overall: %@   LuaJIT: %@   FFI: %@   JIT: %@",
                    self.overallStatus, @"LuaJIT 2.1", self.ffiStatus, self.jitStatus];

    UIScrollView *scrollView = [UIScrollView new];
    UIStackView *lines = [UIStackView new];
    lines.axis = UILayoutConstraintAxisVertical;
    lines.spacing = 4.0;
    for (ANDTestResult *result in self.results) {
        UILabel *line = [UILabel new];
        line.numberOfLines = 0;
        line.font = [UIFont monospacedSystemFontOfSize:11.0 weight:UIFontWeightRegular];
        line.text = [NSString stringWithFormat:@"%@  %@  (%.1f ms)  %@",
                     [result statusString], result.name, result.durationMs, result.message];
        line.textColor = (result.status == ANDTestStatusPass) ? [UIColor systemGreenColor]
                       : (result.status == ANDTestStatusFail) ? [UIColor systemRedColor]
                       : [UIColor systemOrangeColor];
        [lines addArrangedSubview:line];
    }
    if (self.reportError.length > 0) {
        UILabel *errorLine = [UILabel new];
        errorLine.numberOfLines = 0;
        errorLine.font = [UIFont systemFontOfSize:12.0];
        errorLine.textColor = [UIColor redColor];
        errorLine.text = [NSString stringWithFormat:@"Report error: %@", self.reportError];
        [lines addArrangedSubview:errorLine];
    }
    [scrollView addSubview:lines];

    UIButton *exportButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [exportButton setTitle:@"Export Report" forState:UIControlStateNormal];
    [exportButton addTarget:self
                     action:@selector(exportReport:)
           forControlEvents:UIControlEventTouchUpInside];
    exportButton.enabled = (self.reportURL != nil);

    NSArray<UIView *> *views = @[title, versionLabel, summary, scrollView, exportButton];
    for (UIView *view in views) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [self.view addSubview:view];
    }

    UILayoutGuide *guide = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [title.topAnchor constraintEqualToAnchor:guide.topAnchor constant:12.0],
        [title.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16.0],
        [title.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16.0],
        [versionLabel.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:4.0],
        [versionLabel.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16.0],
        [versionLabel.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16.0],
        [summary.topAnchor constraintEqualToAnchor:versionLabel.bottomAnchor constant:10.0],
        [summary.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16.0],
        [summary.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16.0],
        [scrollView.topAnchor constraintEqualToAnchor:summary.bottomAnchor constant:12.0],
        [scrollView.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16.0],
        [scrollView.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16.0],
        [scrollView.bottomAnchor constraintEqualToAnchor:exportButton.topAnchor constant:-12.0],
        [exportButton.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16.0],
        [exportButton.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16.0],
        [exportButton.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor constant:-16.0],
        [exportButton.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
        [lines.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
        [lines.leadingAnchor constraintEqualToAnchor:scrollView.leadingAnchor],
        [lines.trailingAnchor constraintEqualToAnchor:scrollView.trailingAnchor],
        [lines.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
        [lines.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor]
    ]];
}

- (void)exportReport:(id)sender {
    if (self.reportURL == nil) {
        return;
    }
    UIActivityViewController *controller = [[UIActivityViewController alloc]
        initWithActivityItems:@[self.reportURL]
        applicationActivities:nil];
    if (controller.popoverPresentationController != nil) {
        controller.popoverPresentationController.sourceView = sender;
        controller.popoverPresentationController.sourceRect = [sender bounds];
    }
    [self presentViewController:controller animated:YES completion:nil];
}

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
    DeviceRuntimeTests *tests = [DeviceRuntimeTests new];
    [tests runAllTests];

    NSError *reportError = nil;
    NSMutableArray<NSDictionary<NSString *, id> *> *reportTests = [NSMutableArray array];
    for (ANDTestResult *result in tests.results) {
        [reportTests addObject:[result reportDictionary]];
    }
    NSURL *reportURL = [ReportGenerator writeReportWithTests:reportTests
                                                    overall:tests.overallStatus
                                                   jitStatus:tests.jitStatus
                                                   ffiStatus:tests.ffiStatus
                                                      error:&reportError];

    ANDTestViewController *root = [ANDTestViewController new];
    root.results = tests.results;
    root.overallStatus = tests.overallStatus;
    root.jitStatus = tests.jitStatus;
    root.ffiStatus = tests.ffiStatus;
    root.reportURL = reportURL;
    root.reportError = reportError.localizedDescription;

    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.window.rootViewController = root;
    [self.window makeKeyAndVisible];
    return YES;
}

@end
