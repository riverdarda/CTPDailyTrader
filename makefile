CXX=g++
BIN=$(addprefix bin/,trader)
LIB=$(addprefix lib/lib,util.so)
CTPLIBS=$(addprefix CTP_API/,thostmduserapi.so thosttraderapi.so)

all:$(LIB) $(BIN)
	$(info $(BIN))
bin/trader:cpp/main.cpp $(LIB) include/CustomTrade.h include/Kline.h
	mkdir -p bin
	$(CXX) $< $(CTPLIBS) -o $@ -I include -I CTP_API -lutil -L lib -lpthread
lib/libutil.so:cpp/util.cpp include/util.h
	mkdir -p lib
	$(CXX) $< -fpic -shared -g -o $@ -I include
clean:
	rm -rf bin/trader
	rm -rf $(LIB)
