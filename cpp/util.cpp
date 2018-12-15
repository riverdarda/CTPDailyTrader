#include "util.h"

int code_convert(char *inbuf,size_t inlen,char *outbuf, size_t outlen)
{
  iconv_t cd;
  int rc;
  char **pin = &inbuf;
  char **pout = &outbuf;
  char from_charset[] = "gb2312";
  char to_charset[] = "utf-8";
  cd = iconv_open(to_charset,from_charset);
  if (cd==0) return -1;
  memset(outbuf,0,outlen);
  if (iconv(cd,pin,&inlen,pout,&outlen)==-1) return -1;
  iconv_close(cd);
  return 0;
}

int Time2Displace(std::string Time) {
  int hour = (Time[0] - '0') * 10 + Time[1] - '0';
  int minute = (Time[3] - '0') * 10 + Time[4] - '0';
  int second = (Time[6] - '0') * 10 + Time[7] - '0';
  int displace;
  if (hour < 21) {
    displace = (hour + 24 - 21) * 3600 + minute * 60 + second;
  } else {
    displace = (hour - 21) * 3600 + minute * 60 + second;
  }
  return displace;
}

int tm2Displace(tm *ltm) {
  int displace;
  if (ltm -> tm_hour < 21) {
    displace = (ltm -> tm_hour + 24 -21 ) * 3600 + ltm -> tm_min * 60 + ltm ->tm_sec;
  } else {
    displace = (ltm -> tm_hour -21 ) * 3600 + ltm -> tm_min * 60 + ltm ->tm_sec;
  }
  return displace;
}
