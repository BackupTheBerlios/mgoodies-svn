call f:\PlatformSDK\SetEnv /X64 /RETAIL
nmake -f Skype_protocol.mak  CFG="Win64 Release" clean
nmake -f Skype_protocol.mak  CFG="Win64 Release"
