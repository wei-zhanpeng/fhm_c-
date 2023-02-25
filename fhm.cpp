#include <ctime>
#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#define FILE_NAME "db.txt"
#include <algorithm>
using namespace std;
//time clk
clock_t  Begin, End;

double duration;
//#define DDR_ADDR 0x00100000   //存储压缩数据库的开始地址
//#define DDR_ADDRitemListsize 0x00010000  //存储 itemList.size()和NumberOfRows的地址
//#define DDR_ADDRNumberOfRows 0x00010005  //存储NumberOfRows的地址
//#define DDR_ADDRAuub 0x00010010    //存储mapItemToTwu的地址
int minUtility = 40;
int total_read = 0;
unordered_map<int, int> mapItemToTwu;
unordered_map<int,unordered_map<int,int>> mapFMAP;
unordered_map<int, int> mapItemToTwuList;  //存储为<item,twu>格式
int TxtIsEnd = 0;
int candidateCount = 0;
short int prefix[200];
//int NumberOfTids = 0;
//unsigned int MaxItemNum = 0; //item的数目，用来代替revisedTransaction的最大项目数

class Pair
{
public:
		int item = 0;
		int utility = 0;
};

class Element
{
public:
	int tid = 0;

	int iutils = 0;

	int rutils = 0;

	Element(int tid,int iutils,int rutils)
	{
		this->tid = tid;
		this->iutils = iutils;
		this->rutils = rutils;

	}

};


class UtilityList
{
public:
	int item = 0;
	int sumIutils = 0;
	int sumRutils = 0;

	list<Element> elements;

	UtilityList(int item)
	{
		this->item = item;
	}

	void addElement(Element element)
	{
		this->sumIutils += element.iutils;
		this->sumRutils += element.rutils;
		elements.push_back(element);
	}

	bool operator < (UtilityList b){

		unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(this->item);
		unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(b.item);

		return (pos1->second) < (pos2->second);
	}


};



bool itemListAscendingOrder(int val1 , int val2)
{
	unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(val1);
	unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(val2);

	return (pos1->second) < (pos2->second);
}
// = 的情况


bool revisedTransactionAscendingOrder(Pair pair1 , Pair pair2)
{
	unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(pair1.item);
	unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(pair2.item);

	return (pos1->second) < (pos2->second);
}

bool listOfUtilityListsAscendingOrder(UtilityList ul1 , UtilityList ul2)
{
	return ul1 < ul2;
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


/*construct
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
				unordered_map<int,int> mapTWUF = mapFMAP.find(X.item)->second;
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
}*/
int main()
{
		//init_platform();
		int prefixlength = 0;
		ifstream fin ;
		fin.open("C:\\Users\\xidian\\Desktop\\fhm\\fhm.txt",ios::in);
		if(!fin.is_open()) { cout << "error" <<endl; return 0 ;} 

		Begin = clock();
		int NumberOfRows = 0;
		string ReadDataLine;
		//first database scan to build item-twu map
		do
		{
			getline(fin,ReadDataLine);
			vector<string> split = stringSplit(ReadDataLine,':');
			vector<string> items = stringSplit(split.front(),' ');
			int transactionUtility = atoi(split[1].c_str());

			for(vector<string>::iterator it = items.begin(); it != items.end(); it++)
			{
				unordered_map<int, int>::iterator pos = mapItemToTwu.find(atoi((*it).c_str()));
				//if(it==items.begin()) cout << mapItemToTwu.max_size() << endl;
				if (pos != mapItemToTwu.end())
				{
					pos->second += transactionUtility;
				}
				else
				{
					mapItemToTwu.insert(pair<int,int>(atoi((*it).c_str()),transactionUtility));
				}
				//if(pos->second==2^32-1) cout << "overlap" <<endl;
			}

			items.clear();
			NumberOfRows++;
			cout << "this is "<< NumberOfRows << "line" << endl;
		}while(!fin.eof());
		
		list<UtilityList> listOfUtilityLists;
		unordered_map<int,UtilityList> mapItemToUtilityList;
		for(unordered_map<int,int>::iterator it = mapItemToTwu.begin(); it != mapItemToTwu.end(); it++)
		{

			if((it->second) >= minUtility)
			{
				UtilityList ulist = UtilityList(it->first);
				mapItemToUtilityList.insert(pair<int,UtilityList>(it->first,ulist));
				listOfUtilityLists.push_back(ulist);
			}
		}

		listOfUtilityLists.sort(listOfUtilityListsAscendingOrder);
		//ItemListSize = itemList.size();
		total_read = 0;
		TxtIsEnd = 0;// 1:txt is end
		fin.seekg(0);
		//second database scan
		do{
			getline(fin,ReadDataLine);
			vector<string> split = stringSplit(ReadDataLine,':');
			vector<string> items = stringSplit(split.front(),' ');
			vector<string> utilityValues = stringSplit(split.back(),' ');
			//保存twu大于minutil的 项-效用对
			list<Pair> revisedTransaction;
			int tid = 0;
			int remainingUtility = 0;
			//newTWU
			int newTWU = 0;

			for(int i = 0;i < items.size();i++)
			{
				Pair pair;
				pair.item = atoi(items[i].c_str());
				pair.utility = atoi(utilityValues[i].c_str());
				unordered_map<int, int>::iterator pos = mapItemToTwu.find(pair.item);
				if((pos->second) >= minUtility)
				{
					revisedTransaction.push_back(pair);
					remainingUtility += pair.utility;
					newTWU += pair.utility;
				}
			}
			//save space
			items.clear();
			utilityValues.clear();
			//将剩余项按照twu大小升序排列
			revisedTransaction.sort(revisedTransactionAscendingOrder);
			//第二次数据库扫描构建 每个项 的效用列表以及eucs结构
			for(list<Pair>::iterator it = revisedTransaction.begin(); it != revisedTransaction.end(); it++){

				Pair pair = *it;
				//去除当前项，剩余项的效用和
				remainingUtility = remainingUtility - pair.utility;
				//在效用列表和项的map中，找到当前项的map,后续添加 项-效用-剩余效用 对
				unordered_map<int,UtilityList>::iterator tem = mapItemToUtilityList.find(pair.item);
				UtilityList utilityListOfItem = tem->second;

				Element element = Element(tid,pair.utility,remainingUtility);

				utilityListOfItem.addElement(element);
				//ecus结构的构建
				unordered_map<int,unordered_map<int,int>>::iterator temp = mapFMAP.find(pair.item);
				unordered_map<int,int>& mapFMAPItem = temp->second;
				if(temp == mapFMAP.end()){
				      int item = pair.item;
				      mapFMAP.insert({item,mapFMAPItem});
				 }else mapFMAPItem = temp->second;

				 for(list<Pair>::iterator it2=it;it2!=revisedTransaction.end();++it2){
				      Pair pairAfter = *it2;
				      unordered_map<int,int>::iterator temp1 = mapFMAPItem.find(pairAfter.item);
					  if(temp1 == mapFMAPItem.end()){
						  mapFMAPItem.insert({pairAfter.item,newTWU});
					  }
					  else{
						  temp1->second  = temp1->second + newTWU;
					  }
				 }
				 mapFMAP.insert({pair.item,mapFMAPItem});
				 //eucs构建
			}
			
			tid++;
			revisedTransaction.clear();
		}while(!fin.eof());
		// close file
		fin.close();

	mapItemToTwu.clear();
	UtilityList pUL(0);
	//
	End = clock();
	duration = double(End - Begin) / CLK_TCK;
	printf("duration: %d",duration);
		system("pause");
		return 0;
	//fhm(prefix,prefixlength,pUL,listOfUtilityLists,minUtility);

	return 0;
}



