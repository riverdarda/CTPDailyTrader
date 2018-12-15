import sys
import pandas as pd

HEADER = ['TradingDay',
          'InstrumentID',
          'ExchangeID',
          'ExchangeInstID',
          'LastPrice',
          'PreSettlementPrice',
          'PreClosePrice',
          'PreOpenInterest',
          'OpenPrice',
          'HighestPrice',
          'LowestPrice',
          'Volume',
          'Turnover',
          'OpenInterest',
          'ClosePrice',
          'SettlementPrice',
          'UpperLimitPrice',
          'LowerLimitPrice',
          'PreDelta',
          'CurrDelta',
          'UpdateTime',
          'UpdateMillisec',
          'BidPrice1',
          'BidVolume1',
          'AskPrice1',
          'AskVolume1',
          'BidPrice2',
          'BidVolume2',
          'AskPrice2',
          'AskVolume2',
          'BidPrice3',
          'BidVolume3',
          'AskPrice3',
          'AskVolume3',
          'BidPrice4',
          'BidVolume4',
          'AskPrice4',
          'AskVolume4',
          'BidPrice5',
          'BidVolume5',
          'AskPrice5',
          'AskVolume5',
          'AveragePrice',
          'ActionDay']
products = []
today = sys.argv[1]
if len(sys.argv) > 2:
    yestoday = sys.argv[2]
else:
    yestoday = ""
for line in open('products'):
    products.append(line.strip());
filehandle = open('../CTP_tickrecorder/TickData/%s/%s.txt'%(today[:6], today))

def Instrument2Product(InstrumentID):
    if InstrumentID[1] >= '0' and InstrumentID[1] <= '9':
        ProductID = InstrumentID[0:1]
    else:
        ProductID = InstrumentID[0:2]
    return ProductID

main_inst = {}
for i in range(1000):
    line = filehandle.readline().strip()
    if len(line.strip()) == 0:
        continue
    d = dict(zip(HEADER, line.split('|') ) )
    InstrumentID = d['InstrumentID']
    ProductID = Instrument2Product(InstrumentID)
    PreOpenInterest = float(d['PreOpenInterest'])
    if ProductID in products:
        if main_inst.get(ProductID) is None:
            main_inst[ProductID] = (InstrumentID, PreOpenInterest)
        elif main_inst[ProductID][1] < PreOpenInterest:
            main_inst[ProductID] = (InstrumentID, PreOpenInterest)
if yestoday != "":
    yestoday_main = open('main_instrument/main_%s.txt'%yestoday)
    for InstrumentID in yestoday_main:
        InstrumentID = InstrumentID.strip()
        ProductID = Instrument2Product( InstrumentID )
        if main_inst[ProductID][0] < InstrumentID:
            main_inst[ProductID] = (InstrumentID, -1)

filehandle.close()            

filehandle = open('main_instrument/main_%s.txt'%today,'w')
for item in main_inst.items():
    print(item)
    filehandle.write( item[1][0] + '\n')
filehandle.close()
