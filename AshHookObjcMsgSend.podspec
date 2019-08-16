Pod::Spec.new do |s|
  s.name         = "AshHookObjcMsgSend" # 项目名称
  s.version      = "1.0.0"        # 版本号 与 你仓库的 标签号 对应
  s.license      = { :type => "MIT", :file => "LICENSE" }          # 开源证书
  s.summary      = "hook objc_msgSend" # 项目简介
  s.author       = { "BY" => "ashbass@163.com" } # 作者信息
  s.homepage     = "https://github.com/AshBass/AshHookObjcMsgSend.git" # 仓库的主页
  s.source       = { :git => "https://github.com/AshBass/AshHookObjcMsgSend.git", :tag => "#{s.version}" }#你的仓库地址，不能用SSH地址

  s.requires_arc = true # 是否启用ARC
  s.platform     = :ios, "9.0" #平台及支持的最低版本
  #支持的框架
  s.frameworks = [
    "UIKit", 
    "Foundation"
    ]
    
  s.public_header_files = 'AshHookObjcMsgSend/*.h'
  s.source_files = 'AshHookObjcMsgSend/*.{h,c}'

end