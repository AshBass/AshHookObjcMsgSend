
#import "ViewController.h"
#include "AshHookObjcMsgSend.h"

@interface ViewController ()

@end

@implementation ViewController

-(void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self test];
    int num = getRecordNumber();
    for (int i = 0; i < num; ++i) {
        AshCallRecord *callRecord = getCallRecords();
        AshCallRecord record = callRecord[i];
        NSLog(@"%@ %@ 耗时 : %llu",NSStringFromClass(record.class),NSStringFromSelector(record.selector),record.time/1000000);
    }
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

-(void)test {
    sleep(3);
}


@end
