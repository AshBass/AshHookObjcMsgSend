# AshHookObjcMsgSend

使用fishhook hook objc_msgSend ，然后统计方法耗时

仅支持 arm64架构 （即4s、模拟器不能使用）

具体步骤
- 使用 fishhook hook objc_msgSend
- 由于 objc_msgSend 是用汇编编写，所以要对 x0 ~ x7进行操作（demo只支持arm64）
- 记录下方法调用者、方法选择器、返回地址。存储进 pthread 标志中，使其与线程一一对应。
- 调用原来的 objc_msgSend 方法
- 返回原来的 objc_msgSend 的返回地址

补上我的博客地址 https://ashbass.cn
