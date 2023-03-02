#include <ctime>
#include <stdio.h>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include "fhmheader.h"
#define FILE_NAME "db.txt"
#define ENABLE_LA_PRUNE true
#include <algorithm>
using namespace std;
//time clk
clock_t  Begin, End;
double duration;
//#define DDR_ADDR 0x00100000   //存储压缩数据库的开始地址
//#define DDR_ADDRitemListsize 0x00010000  //存储 itemList.size()和NumberOfRows的地址
//#define DDR_ADDRNumberOfRows 0x00010005  //存储NumberOfRows的地址
//#define DDR_ADDRAuub 0x00010010    //存储mapItemToTwu的地址
int minUtility = 440;
int huiCount = 0;
//
unordered_map<int, long> mapItemToTwu;
unordered_map<int,unordered_map<int,long>> mapFMAP;
unordered_map<int, long> mapItemToTwuList;  //存储为<item,twu>格式

int candidateCount = 0;
int prefix[200];
//int NumberOfTids = 0;
//unsigned int MaxItemNum = 0; //item的数目，用来代替revisedTransaction的最大项目数

static bool itemListAscendingOrder(int val1 , int val2)
{
	unordered_map<int, long>::iterator pos1 = mapItemToTwu.find(val1);
	unordered_map<int, long>::iterator pos2 = mapItemToTwu.find(val2);

	return (pos1->second) < (pos2->second);
}
// = 的情况


static bool revisedTransactionAscendingOrder(Pair &pair1 , Pair &pair2)
{
	unordered_map<int, long>::iterator pos1 = mapItemToTwu.find(pair1.item);
	unordered_map<int, long>::iterator pos2 = mapItemToTwu.find(pair2.item);

	return (pos1->second) < (pos2->second);
}

static bool listOfUtilityListsAscendingOrder(int a , int b)
{
	unordered_map<int, long>::iterator pos1 = mapItemToTwu.find(a);
	unordered_map<int, long>::iterator pos2 = mapItemToTwu.find(b);
	return pos1->second < pos1->second;
}


//按照输入的字符分割string类型的数据
static vector<string> stringSplit(const string& str, char delim)
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

static void findElementWithTID(UtilityList &ulist,int tid,Element &e){

	vector<Element> list =ulist.elements;
	//list.assign(ulist.elements.begin(),ulist.elements.end());
	int first = 0;
	int last = list.size() - 1;

	while(first<=last){
		int middle = (unsigned)(first + last) >> 1;
		if(list[middle].tid < tid){
            first = middle + 1;  //  the itemset compared is larger than the subset according to the lexical order
        }
        else if(list[middle].tid > tid){
            last = middle - 1; //  the itemset compared is smaller than the subset  is smaller according to the lexical order
        }
        else{
            e.is_exist = 1;
			e = list[middle];
			return;
        }
	}
	return ;
}

//write out
static void writeOut(int prefix[],int prefixLength,int item,long utility,fstream &file){
	huiCount++;

	string buf;
	string tem;
	for(int i=0;i<prefixLength;i++){
		tem = to_string(prefix[i]);
		buf.append(tem);
		buf.append(' ',1);
	}
	tem = to_string(item);
	buf.append(tem);
	buf.append(" #UTIL: ");
	tem = to_string(utility);
	buf.append(tem);

	file << buf << endl;

}


//construct
static UtilityList construct(UtilityList &P,UtilityList &px,UtilityList &py,int minUtility){

	UtilityList pxyUL ;
	pxyUL.item = py.item;
	
	long totalUtility = px.sumIutils + px.sumRutils;
	
	for(Element ex : px.elements){
		Element ey;
		findElementWithTID(py,ex.tid,ey);
		if(ey.is_exist==0){
			if(ENABLE_LA_PRUNE){
				totalUtility -= (ex.iutils+ex.rutils);
				if(totalUtility < minUtility) {
					return pxyUL;
			}
		}
		}
		continue;
	
		//if prefix p is null
		if( P.is_exist == 0 ){
			Element eXY ;
			eXY.set(ex.tid,ex.iutils+ey.iutils,ey.rutils);
			pxyUL.addElement(eXY);
		}else{

			Element e ;
			findElementWithTID(P,ex.tid,e);
			if(e.is_exist==0){
				Element eXY ;
				eXY.set(ex.tid,ex.iutils+ey.iutils-e.iutils,ey.rutils);
				pxyUL.addElement(eXY);
			}
		}
		s
	}
	pxyUL.is_exist=1;
	return pxyUL;
	

}
//dec
//只存储ulist对应的item,其他不保存在其中
static vector<UtilityList> listOfUtilityLists;
unordered_map<int,UtilityList> mapItemToUtilityList;

//fhm
static void fhm(int prefix[],int prefixlength,UtilityList &pUL,vector<UtilityList> &ULs,int minUtility,fstream &file){
	for(vector<UtilityList>::iterator it=ULs.begin();it!=ULs.end();it++){
		UtilityList &X = mapItemToUtilityList[it->item];
		vector<UtilityList> exULs;
		
		if(X.sumIutils >= minUtility){
			writeOut(prefix,prefixlength,X.item,X.sumIutils,file);
		}

		if(X.sumIutils+X.sumRutils >= minUtility){
			for(vector<UtilityList>::iterator it2=it+1;it2!=ULs.end();it2++){
				UtilityList Y = *it2;
				//eucs prune
				//unordered_map<short int,unordered_map<short int,int>>::iterator mapTWUF = mapFMAP.find(x.item);
				unordered_map<int,long> mapTWUF = mapFMAP.find(X.item)->second;//mapFMAP[X.item];
				if(mapTWUF.size() != 0){
					auto twuF = mapTWUF.find(Y.item);
					if(twuF == mapTWUF.end() || twuF->second <= minUtility) continue;
				}
				candidateCount++;

				//construct
				UtilityList temp = construct(pUL,X,Y,minUtility);
				if(temp.is_exist==1){
					exULs.push_back(temp);
				}
			}

		}
		prefix[prefixlength] = X.item;

		fhm(prefix,prefixlength+1,X,exULs,minUtility,file);
	}
}



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
				unordered_map<int, long>::iterator pos = mapItemToTwu.find(atoi((*it).c_str()));
				//if(it==items.begin()) cout << mapItemToTwu.max_size() << endl;
				if (pos != mapItemToTwu.end())
				{
					pos->second += transactionUtility;
				}
				else
				{
					mapItemToTwu.insert(pair<int,long>(atoi((*it).c_str()),transactionUtility));
				}
				//if(pos->second==2^32-1) cout << "overlap" <<endl;
			}

			items.clear();
			NumberOfRows++;
			//cout << "this is "<< NumberOfRows << "line" << endl;
		}while(!fin.eof());
		
		int idxOfutil=0;
		for(unordered_map<int,long>::iterator it = mapItemToTwu.begin(); it != mapItemToTwu.end(); it++)
		{
			UtilityList ulist ;
			if((it->second) >= minUtility)
			{
				//ulist.item = it->first;
				//mapItemToUtilityList.insert({it->first,ulist});
				//1
				//list listofutilitylists
				//listOfUtilityLists.push_front(ulist);
				//mapItemToUtilityList.insert({it->first,(UtilityList *)&(*listOfUtilityLists.begin())});
				//2
				//vector listofutilitylists
				//listOfUtilityLists.push_back(ulist);
				//mapItemToUtilityList.insert({it->first,listOfUtilityLists[idxOfutil]});idxOfutil++;
				//3
				ulist.item = it->first;
				listOfUtilityLists.push_back(ulist);	
			}
		}
		//idxOfutil=0;
		//对效用列表按照item的twu升序排列
		sort(listOfUtilityLists.begin(),listOfUtilityLists.end(),listOfUtilityListsAscendingOrder);
		//listOfUtilityLists.sort(listOfUtilityListsAscendingOrder);
		//ItemListSize = itemList.size();
		//tid初始化为0
		int tid = 0;
		fin.seekg(0);
		//second database scan
		do{
			getline(fin,ReadDataLine);
			vector<string> split = stringSplit(ReadDataLine,':');
			vector<string> items = stringSplit(split.front(),' ');
			vector<string> utilityValues = stringSplit(split.back(),' ');
			//保存twu大于minutil的 项-效用对
			vector<Pair> revisedTransaction;
			
			int remainingUtility = 0;
			//newTWU
			int newTWU = 0;

			for(int i = 0;i < items.size();i++)
			{
				Pair pair;
				pair.item = atoi(items[i].c_str());
				pair.utility = atoi(utilityValues[i].c_str());
				unordered_map<int, long>::iterator pos = mapItemToTwu.find(pair.item);
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
			sort(revisedTransaction.begin(),revisedTransaction.end(),revisedTransactionAscendingOrder);
			//revisedTransaction.sort(revisedTransactionAscendingOrder);
			UtilityList itemUl;
			//第二次数据库扫描构建 每个项 的效用列表以及eucs结构
			for(vector<Pair>::iterator it = revisedTransaction.begin(); it != revisedTransaction.end(); it++){

				Pair pair = *it;
				//去除当前项，剩余项的效用和
				remainingUtility = remainingUtility - pair.utility;
				//在效用列表和项的map中，找到当前项的map,后续添加 项-效用-剩余效用 对
				//unordered_map<int,UtilityList&>::iterator tem = mapItemToUtilityList.find(pair.item);
				Element element ;
				element.set(tid,pair.utility,remainingUtility);
				//1
				//UtilityList tem = mapItemToUtilityList.find(pair.item)->second;
				//tem.addElement(element);
				//2
				//listOfUtilityLists[idxOfutil].addElement(element);idxOfutil++;
				//itemUl = find(listOfUtilityLists.begin(),listOfUtilityLists.end(),it->item)
				//*itemUl.addElement(element);
				//(mapItemToUtilityList.find(pair.item)->second).addElement(element);
				//4
				itemUl.addElement(element);
				if(mapItemToUtilityList.find(pair.item) != mapItemToUtilityList.end()) {
					mapItemToUtilityList[pair.item].addElement(element);
				}else{
					mapItemToUtilityList.insert({pair.item,itemUl});
				}
				//3
				//utilityListOfItem.addElement(element);
				//mapItemToUtilityList.insert({pair.item,utilityListOfItem});
				//ecus结构的构建
				unordered_map<int,unordered_map<int,long>>::iterator temp = mapFMAP.find(pair.item);
				unordered_map<int,long> mapFMAPItem ;//= temp->second;
				if(temp == mapFMAP.end()){//error
				      int item = pair.item;
					  //mapFMAP.insert(make_pair(item,mapFMAPItem));
				      mapFMAP.insert({item,mapFMAPItem});
				 }else mapFMAPItem = temp->second;

				 for(vector<Pair>::iterator it2=it+1;it2!=revisedTransaction.end();it2++){
				      Pair pairAfter = *it2;
				      unordered_map<int,long>::iterator temp1 = mapFMAPItem.find(pairAfter.item);
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
			//每行读取完成tid++
			tid++;
			revisedTransaction.clear();
		}while(!fin.eof());
		// close file
		fin.close();

	mapItemToTwu.clear();
	UtilityList pUL;
	//创建文件来存储hui
	fstream file;
	file.open("C:\\Users\\xidian\\Desktop\\fhm\\hui.txt",ios::out); //以
	file << "hui" << endl;
	if(!file.is_open()) cout << "1error open file" <<endl;
	file.close(); //关闭文件

	file.open("C:\\Users\\xidian\\Desktop\\fhm\\hui.txt", ios::app); //以追加模式打开文件
	if(!file.is_open()) cout << "2error open file" <<endl;
	//
	End = clock();
	duration =double((End - Begin)/CLK_TCK);
	printf("duration: %d",duration);
	
	fhm(prefix,prefixlength,pUL,listOfUtilityLists,minUtility,file);
	file.close();
	
	
	return 0;
}






