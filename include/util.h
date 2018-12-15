#ifndef __UTIL_H__
#define __UTIL_H__

#include <string.h>
#include <iconv.h>
#include <string>
#include <ctime>

int code_convert(char *inbuf,size_t inlen,char *outbuf, size_t outlen);
int Time2Displace(std::string Time);
int tm2Displace(tm *ltm);
#endif
