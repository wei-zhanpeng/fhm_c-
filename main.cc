using namespace std;
#include <ctime>
#include <stdio.h>
#include "xil_cache.h"
#include "ff.h"
#include <string>
#include <map>
#include "xil_io.h"
#include <vector>
#include <unordered_map>
#include <list>
#include "xparameters.h"
#define FILE_NAME "fhm.txt"
#include <algorithm>
//time clk
clock_t  Begin, End;

double duration;
//#define DDR_ADDR 0x00100000   //存储压缩数据库的开始地址
//#define DDR_ADDRitemListsize 0x00010000  //存储 itemList.size()和NumberOfRows的地址
//#define DDR_ADDRNumberOfRows 0x00010005  //存储NumberOfRows的地址
//#define DDR_ADDRAuub 0x00010010    //存储mapItemToTwu的地址
int minUtility = 40;
int total_read = 0;
FIL fil;		/* File object */
unordered_map<short int, int> mapItemToTwu;
unordered_map<short int,unordered_map<short int,int>> mapFMAP;
unordered_map<short int, int> mapItemToTwuList;  //存储为<item,twu>格式
short int TxtIsEnd = 0;
int candidateCount = 0;
short int prefix[200];
//int NumberOfTids = 0;
//unsigned int MaxItemNum = 0; //item的数目，用来代替revisedTransaction的最大项目数

class Pair
{
public:
		short int item = 0;
		short int utility = 0;
};

class Element
{
public:
	short int tid = 0;

	short int iutils = 0;

	short int rutils = 0;

	Element(short int tid,short int iutils,short int rutils)
	{
		this->tid = tid;
		this->iutils = iutils;
		this->rutils = rutils;

	}

};


class UtilityList
{
public:
	short int item = 0;
	short int sumIutils = 0;
	short int sumRutils = 0;

	list<Element> elements;

	UtilityList(short int item)
	{
		this->item = item;
	}

	void addElement(Element element)
	{
		this->sumIutils += element.iutils;
		this->sumRutils += element.rutils;
		elements.push_back(element);
	}


};



bool itemListAscendingOrder(short int val1 , short int val2)
{
	unordered_map<short int, int>::iterator pos1 = mapItemToTwu.find(val1);
	unordered_map<short int, int>::iterator pos2 = mapItemToTwu.find(val2);

	return (pos1->second) < (pos2->second);
}
// = 的情况


bool revisedTransactionAscendingOrder(Pair pair1 , Pair pair2)
{
	unordered_map<short int, int>::iterator pos1 = mapItemToTwu.find(pair1.item);
	unordered_map<short int, int>::iterator pos2 = mapItemToTwu.find(pair2.item);

	return (pos1->second) < (pos2->second);
}

bool listOfUtilityListsAscendingOrder(UtilityList ul1 , UtilityList ul2)
{
	unordered_map<short int, int>::iterator pos1 = mapItemToTwuList.find(ul1.item);
	unordered_map<short int, int>::iterator pos2 = mapItemToTwuList.find(ul2.item);

	return (pos1->second) < (pos2->second);
}





//按照输入的字符分割string类型的数据
vector<string> stringSplit(const string& str, char delim)
{
    size_t previous = 0;
    size_t current = str.find(delim);
    vector<string> elems;
    while (current != string::npos)
    {
        if (current > previous)
        {
            elems.push_back(str.substr(previous, current - previous));
        }
        previous = current + 1;
        current = str.find(delim, previous);
    }
    if (previous != str.size())
    {
        elems.push_back(str.substr(previous));
    }
    return elems;
}


//SD卡读取数据
string ReadLineData()
{
	UINT NumBytesRead;
	string ReadDataLine;
	char ReadChar;
	try
	{
		while(1)
			{
				TxtIsEnd = f_eof(&fil);
				if(TxtIsEnd)
				{
					return ReadDataLine;
				}
				f_lseek(&fil,total_read);
				f_read(&fil, &ReadChar, sizeof(ReadChar), &NumBytesRead);
				if(ReadChar=='\n')
				{
					total_read++;
					return ReadDataLine;
				}
				ReadDataLine+=ReadChar;
				total_read++;

			}
	}
	catch(std::bad_alloc)
	{
		TxtIsEnd = 1;
		//return ReadDataLine;
	}
}
//construct
UtilityList construct(UtilityList &P,UtilityList &px,UtilityList &py,int minUtility){


}
//fhm
void fhm(short int prefix[],int prefixlength,UtilityList &pUL,list<UtilityList> &ULs,int minUtility){
	for(list<UtilityList>::iterator it=ULs.begin();it!=ULs.end();it++){
		UtilityList X = *it;
		list<UtilityList> exULs;

		if(X.sumIutils >= minUtility){
			//write out(prefix,prefixlength,x.item,x.sumIutils);
		}

		if(X.sumIutils+X.sumRutils >= minUtility){
			for(list<UtilityList>::iterator it2=it;it2!=ULs.end();++it2){
				UtilityList Y = *it2;
				//eucs prune
				//unordered_map<short int,unordered_map<short int,int>>::iterator mapTWUF = mapFMAP.find(x.item);
				unordered_map<short int,int> mapTWUF = mapFMAP.find(X.item)->second;
				if(mapTWUF.size() != 0){
					auto twuF = mapTWUF.find(Y.item);
					if(twuF == mapTWUF.end() || twuF->second <= minUtility) continue;
				}
				candidateCount++;

				//construct
				UtilityList temp = construct(pUL,X,Y,minUtility);
				if(sizeof(temp) == 1){
					exULs.push_back(temp);
				}
			}

		}
		prefix[prefixlength] = X.item;

		fhm(prefix,prefixlength+1,X,exULs,minUtility);
	}
}

int main()
{
		//init_platform();
		int prefixlength = 0;
		FATFS fatfs;
		FRESULT Res;
		string ReadDataLine;
		Res = f_mount(&fatfs, "0:/", 0);
		if (Res != FR_OK) xil_printf( "Could not mount SD card.");
		xil_printf("SD Card Mounted successfully\n");
		Res = f_open(&fil, FILE_NAME, FA_READ | FA_OPEN_EXISTING);
		if (Res) throw Res;
		//xil_printf("FILE Opened successfully\n");
		Res = f_lseek(&fil, 0);
		if (Res) throw "Failed to seek opened file.";

		Begin = clock();
		short int NumberOfRows = 0;
	try
		{
		do
		{
			ReadDataLine = ReadLineData();
			vector<string> split = stringSplit(ReadDataLine,':');
			ReadDataLine.clear();
			vector<string> items = stringSplit(split.front(),' ');
			string transactionUtility = split[1];

			for(vector<string>::iterator it = items.begin(); it != items.end(); it++)
			{
				unordered_map<short int, int>::iterator pos = mapItemToTwu.find(atoi((*it).c_str()));
				if (pos != mapItemToTwu.end())
				{
					pos->second += (short int)atoi(transactionUtility.c_str());
				}
				else
				{
					mapItemToTwu.insert(pair<short int,int>(atoi((*it).c_str()),(short int)atoi(transactionUtility.c_str())));
				}
			}
			items.clear();
			NumberOfRows++;
		}while(!TxtIsEnd);
	}
	catch(std::bad_alloc)
	{
		//cout<<"Error"<<endl;
	}
		list<UtilityList> listOfUtilityLists;
		unordered_map<short int,UtilityList> mapItemToUtilityList;
		for(unordered_map<short int,int>::iterator it = mapItemToTwu.begin(); it != mapItemToTwu.end(); it++)
		{

			if((it->second) >= minUtility)
			{
				UtilityList ulist = UtilityList(it->first);
				mapItemToUtilityList.insert(pair<short int,UtilityList>(it->first,ulist));
				listOfUtilityLists.push_back(ulist);
			}
		}

		listOfUtilityLists.sort(listOfUtilityListsAscendingOrder);
		//ItemListSize = itemList.size();
		total_read = 0;
		TxtIsEnd = 0;// 1:txt is end
		Res = f_lseek(&fil, 0);
		if (Res) throw "Failed to seek opened file.";


	try{
		do{
			ReadDataLine = ReadLineData();
			vector<string> split = stringSplit(ReadDataLine,':');
			ReadDataLine.clear();
			vector<string> items = stringSplit(split.front(),' ');
			vector<string> utilityValues = stringSplit(split.back(),' ');

			list<Pair> revisedTransaction;
			int tid = 0;
			int remainingUtility = 0;
			int newTWU = 0;

			for(unsigned short int i = 0;i < items.size();i++)
			{
				Pair pair;
				pair.item = atoi(items[i].c_str());
				pair.utility = atoi(utilityValues[i].c_str());
				unordered_map<short int, int>::iterator pos = mapItemToTwu.find(pair.item);
				if((pos->second) >= minUtility)
				{
					revisedTransaction.push_back(pair);
					remainingUtility += pair.utility;
					newTWU += pair.utility;
				}
			}
			items.clear();
			utilityValues.clear();
			revisedTransaction.sort(revisedTransactionAscendingOrder);
			for(list<Pair>::iterator it = revisedTransaction.begin(); it != revisedTransaction.end(); it++){

				Pair pair = *it;

				remainingUtility = remainingUtility - pair.utility;

				unordered_map<short int,UtilityList>::iterator tem = mapItemToUtilityList.find(pair.item);
				UtilityList utilityListOfItem = tem->second;

				Element element = Element(tid,pair.utility,remainingUtility);

				utilityListOfItem.addElement(element);
				//ecus
				unordered_map<short int,unordered_map<short int,int>>::iterator temp = mapFMAP.find(pair.item);
				unordered_map<short int,int>& mapFMAPItem = temp->second;
				if(temp == mapFMAP.end()){
				      short int item = pair.item;
				      mapFMAP.insert({item,mapFMAPItem});
				 }else mapFMAPItem = temp->second;

				 for(list<Pair>::iterator it2=it;it2!=revisedTransaction.end();++it2){
				      Pair pairAfter = *it2;
				      unordered_map<short int,int>::iterator temp1 = mapFMAPItem.find(pairAfter.item);
					  if(temp1 == mapFMAPItem.end()){
						  mapFMAPItem.insert({pairAfter.item,newTWU});
					  }
					  else{
						  temp1->second  = temp1->second + newTWU;
					  }
				 }
				 mapFMAP.insert({pair.item,mapFMAPItem});
			}
			tid++;
			revisedTransaction.clear();

		}while(!TxtIsEnd);
		f_close(&fil);
	}
	catch(std::bad_alloc)
	{
		//cout<<"Error"<<endl;
	}
	mapItemToTwu.clear();
	UtilityList pUL(0);
	//
	End = clock();
	duration = double(End - Begin) / CLK_TCK;
	xil_printf("duration: %d",duration);
		system("pause");
		return 0;
	fhm(prefix,prefixlength,pUL,listOfUtilityLists,minUtility);

	return 0;
}



