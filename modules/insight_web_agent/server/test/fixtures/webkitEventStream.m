/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface StreamProbe : NSObject <WKScriptMessageHandler, WKURLSchemeHandler>
@property NSString *htmlPath;
@end

@implementation StreamProbe
- (void)userContentController:(WKUserContentController *)controller didReceiveScriptMessage:(WKScriptMessage *)message {
    puts([[message.body description] UTF8String]);
    fflush(stdout);
    [[NSApplication sharedApplication] terminate:nil];
}
- (void)webView:(WKWebView *)view startURLSchemeTask:(id<WKURLSchemeTask>)task {
    NSData *data = [NSData dataWithContentsOfFile:self.htmlPath];
    NSURLResponse *response = [[NSURLResponse alloc] initWithURL:task.request.URL
        MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:@"utf-8"];
    [task didReceiveResponse:response];
    [task didReceiveData:data];
    [task didFinish];
}
- (void)webView:(WKWebView *)view stopURLSchemeTask:(id<WKURLSchemeTask>)task {}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        StreamProbe *probe = [StreamProbe new];
        probe.htmlPath = [NSString stringWithUTF8String:argv[1]];
        WKWebViewConfiguration *config = [WKWebViewConfiguration new];
        [config.userContentController addScriptMessageHandler:probe name:@"probe"];
        [config setURLSchemeHandler:probe forURLScheme:@"wry"];
        WKWebView *view = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config];
        [view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"wry://localhost/probe"]]];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            fputs("Timed out waiting for SSE completion in WKWebView\n", stderr);
            exit(1);
        });
        [app run];
    }
    return 0;
}
