#include "CustomTrade.h"

int main(int argc, char **argv)
{
  
  string config_file = "config";
  if (argc > 1) {
    config_file = argv[1];
  }
  parse_config(config_file);
  
  // 产生一个CThostFtdcMdApi实例
  CThostFtdcMdApi *pUserApiMd = CThostFtdcMdApi::CreateFtdcMdApi();
  // 产生一个CThostFtdcTraderApi实例
  CThostFtdcTraderApi *pUserApiTrader = CThostFtdcTraderApi::CreateFtdcTraderApi();
  // 产生一个事件处理的实例
  CSimpleHandler sh(pUserApiTrader, pUserApiMd);
  // 注册一事件处理的实例
  pUserApiMd->RegisterSpi(&sh);
  // 设置交易托管系统服务的地址，可以注册多个地址备用
  pUserApiMd->RegisterFront(MarketFrontAddr);
  // 使客户端开始与后台服务建立连接
  pUserApiMd->Init();
  // 客户端等待报单操作完成
  //WaitForSingleObject(g_hEvent, INFINITE);
  // 释放API实例
  pUserApiMd->Release();
  return 0;
}

