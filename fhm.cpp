#include <ctime>
#include <map>
#include <list>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include <algorithm>
//pcie driver
#include "pcie_api.h"
#include "xdma_public.h"
//fhm 类定义文件
using namespace std;
#include "fhmheader.h"
//加载静态库
#pragma comment(lib, "setupapi")
//
#define FILE_NAME "db.txt"
#define ENABLE_LA_PRUNE true
//
clock_t  Begin, End;
double duration;
int minUtility = 80000;
int huiCount = 0;
unordered_map<int,int> mapItemToTwu;
unordered_map<int,unordered_map<int,int>> mapFMAP;
unordered_map<int, int> mapItemToTwuList;  //存储为<item,twu>格式
static vector<UtilityList> listOfUtilityLists;//只存储ulist对应的item,其他不保存在其中
unordered_map<int,UtilityList> mapItemToUtilityList;
int candidateCount = 0;
int prefix[200];
//item 升序排列
static bool itemListAscendingOrder(int val1 , int val2)
{
	unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(val1);
	unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(val2);

	return (pos1->second) < (pos2->second);
}
// = 的情况
static bool revisedTransactionAscendingOrder(Pair &pair1 , Pair &pair2)
{
	unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(pair1.item);
	unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(pair2.item);

	return (pos1->second) < (pos2->second);
}

static bool listOfUtilityListsAscendingOrder(UtilityList &a , UtilityList &b)
{

	unordered_map<int, int>::iterator pos1 = mapItemToTwu.find(a.item);
	unordered_map<int, int>::iterator pos2 = mapItemToTwu.find(b.item);

	return (pos1->second) < (pos1->second);
}
//pcie输出utilityList
static void UL2FPGA(UtilityList &a,unsigned long &offset,device_file &h2c,unsigned long &len){

	auto &h2c_data = a;
	auto &h2c_ele = h2c_data.elements;
    WriteFile(h2c.h,&(h2c_data),(DWORD)12,&offset,NULL);
	len = len + 12;
	h2c.seek(offset);
	for(vector<Element>::iterator ele=h2c_ele.begin();ele!=h2c_ele.end();ele++){
		WriteFile(h2c.h,&(*ele),(DWORD)12,&offset,NULL);
		h2c.seek(offset);
		len = len + 12;
	}
		
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

	vector<Element> &list =ulist.elements;
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
static void writeOut(int prefix[],int prefixLength,int item,int utility,fstream &file){
	huiCount++;

	string buf;
	string tem;
	for(int i=0;i<prefixLength;i++){
		tem = to_string(prefix[i]);
		buf.append(tem);
		buf.append(" ");
	}
	tem = to_string(item);
	buf.append(tem);
	buf.append(" #UTIL: ");
	tem = to_string(utility);
	buf.append(tem);

	file << buf << endl;

}

//求map最大值
template <class Key, class Value>
std::pair<Key, Value> findMaxValuePair(std::unordered_map<Key, Value> const &x)
{
    return *std::max_element(x.begin(), x.end(),
                             [](const std::pair<Key, Value> &p1,
                                const std::pair<Key, Value> &p2)
                             {
                                 return p1.second < p2.second;
                             });
}
//construct
static UtilityList construct(UtilityList &P,UtilityList &X,UtilityList &Y,int minUtility){

	UtilityList pxyUL;
	pxyUL.item = Y.item;
	UtilityList &px = mapItemToUtilityList[X.item];
	UtilityList &py = mapItemToUtilityList[Y.item];
	
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
			if(e.is_exist==1){
				Element eXY ;
				eXY.set(ex.tid,ex.iutils+ey.iutils-e.iutils,ey.rutils);
				pxyUL.addElement(eXY);
			}
		}
		
	}
	pxyUL.is_exist=1;
	return pxyUL;
	

}
//fhm
static void fhm(int prefix[],int prefixlength,UtilityList &pUL,vector<UtilityList> &ULs,int minUtility,fstream &file){
	for(vector<UtilityList>::iterator it=ULs.begin();it!=ULs.end();it++){
		UtilityList &X = mapItemToUtilityList[it->item];
		X.item = it->item;
		vector<UtilityList> exULs;
		
		if(X.sumIutils >= minUtility){
			writeOut(prefix,prefixlength,X.item,X.sumIutils,file);
		}

		if(X.sumIutils+X.sumRutils >= minUtility){
			for(vector<UtilityList>::iterator it2=it+1;it2!=ULs.end();it2++){
				UtilityList Y = *it2;
				//eucs prune
				//unordered_map<short int,unordered_map<short int,int>>::iterator mapTWUF = mapFMAP.find(x.item);
				unordered_map<int,int> mapTWUF = mapFMAP[X.item];//mapFMAP.find(X.item)->second;
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
		fin.open("C:\\Users\\xidian\\Desktop\\fhm\\retail_utility.txt",ios::in);
		if(!fin.is_open()) { cout << "error" <<endl; return 0 ;} 
        //------------------------------------
		//pcie device 查找
		const auto device_paths = get_device_paths(GUID_DEVINTERFACE_XDMA);
		if (device_paths.empty()) {
            throw std::runtime_error("Failed to find XDMA device!");
        }else{
			for(int i=0;i<device_paths.size();i++){
			 	cout << "find " << device_paths.size() << " XDMA device! \n" << device_paths[i] << endl;
			}
		}
        //front 返回vector第一个元素的引用
        xdma_device xdma(device_paths.front());
        
        if (xdma.is_axi_st()) { // AXI-ST streaming interface
            std::cout << "Detected XDMA AXI-ST design.\n";
        } else { // AXI-MM Memory Mapped interface
            std::cout << "Detected XDMA AXI-MM design.\n";
        }
		//pcie 读写通道创建
        device_file h2c(device_paths[0] + "\\h2c_" + std::to_string(0), GENERIC_WRITE);
        device_file c2h(device_paths[0] + "\\c2h_" + std::to_string(0), GENERIC_READ);
        if (h2c.h == INVALID_HANDLE_VALUE || c2h.h == INVALID_HANDLE_VALUE) {
            std::cout << "Could not find h2c_" << 0 << " and/or c2h_" << 0 << "\n";
        } else {
            std::cout << "Found h2c_" << 0 << " and c2h_" << 0 << ":\n";
        }
        
        //-------------------------------------

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
				if (pos != mapItemToTwu.end())
				{
					pos->second += transactionUtility;
				}
				else
				{
					mapItemToTwu.insert(pair<int,int>(atoi((*it).c_str()),transactionUtility));
				}
			}

			items.clear();
			NumberOfRows++;
		}while(!fin.eof());

		cout << "max twu:" << findMaxValuePair(mapItemToTwu).second << endl;
		
		int idxOfutil=0;
		//构建listofUtilityLists以及mapItemToUtilityList
		for(unordered_map<int,int>::iterator it = mapItemToTwu.begin(); it != mapItemToTwu.end(); it++)
		{
			UtilityList ulist ;
			if((it->second) >= minUtility)
			{
				ulist.item = it->first;
				listOfUtilityLists.push_back(ulist);	
				mapItemToUtilityList.insert({it->first,ulist});
				idxOfutil++;
			}
		}
		cout << "length of listOfUtilityLists: " << idxOfutil << endl;
		//对效用列表按照item的twu升序排列
		sort(listOfUtilityLists.begin(),listOfUtilityLists.end(),listOfUtilityListsAscendingOrder);
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
				unordered_map<int, int>::iterator pos = mapItemToTwu.find(pair.item);
				if(pos != mapItemToTwu.end()){
					if((pos->second) >= minUtility)
					{
						revisedTransaction.push_back(pair);
						remainingUtility += pair.utility;
						newTWU += pair.utility;
					}
				}
				
			}
			//save space
			//items.clear();
			//utilityValues.clear();
			//将剩余项按照twu大小升序排列
			sort(revisedTransaction.begin(),revisedTransaction.end(),revisedTransactionAscendingOrder);
			
			//第二次数据库扫描构建 每个项 的效用列表以及eucs结构
			for(vector<Pair>::iterator it = revisedTransaction.begin(); it != revisedTransaction.end(); it++){

				Pair pair = *it;
				//去除当前项，剩余项的效用和
				remainingUtility = remainingUtility - pair.utility;
				//在效用列表和项的map中，找到当前项的map,后续添加 项-效用-剩余效用 对
				Element element ;
				element.set(tid,pair.utility,remainingUtility);
				//1
				/*UtilityList itemUl;
				itemUl.item = it->item;
				itemUl.addElement(element);
				if(mapItemToUtilityList.find(pair.item) != mapItemToUtilityList.end()) {
					mapItemToUtilityList[pair.item].addElement(element);
				}else{
					mapItemToUtilityList.insert({pair.item,itemUl});
				}*/
				//2
				UtilityList &itemUl = mapItemToUtilityList[pair.item];
				itemUl.addElement(element);
				
				//ecus结构的构建
				unordered_map<int,unordered_map<int,int>>::iterator temp = mapFMAP.find(pair.item);
				unordered_map<int,int> mapFMAPItem ;//= temp->second;
				if(temp == mapFMAP.end()){//error
				      int item = pair.item;
					  //mapFMAP.insert(make_pair(item,mapFMAPItem));
				      mapFMAP.insert({item,mapFMAPItem});
				 }else mapFMAPItem = temp->second;

				 for(vector<Pair>::iterator it2=it+1;it2!=revisedTransaction.end();it2++){
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
			//每行读取完成tid++
			tid++;
			revisedTransaction.clear();
		}while(!fin.eof());
		// close file
		fin.close();

	mapItemToTwu.clear();
	UtilityList pUL;
	//---------------------------------------
	unsigned long offset;//数据传输时的偏移
	unsigned long lenOfUL=0;
	unsigned long off1=0x40000000,off2=0x42000000;
	/*for(auto uli = mapItemToUtilityList.begin();uli!=mapItemToUtilityList.end();uli++){
		auto &ulist = uli->second;
		UL2FPGA(ulist,offset,h2c,lenOfUL);
	}*/
	auto uli = mapItemToUtilityList.begin();
	h2c.reoffset(off1);
	UL2FPGA(uli->second,offset,h2c,lenOfUL);
	h2c.reoffset(off2);
	uli ++;
	UL2FPGA(uli->second,offset,h2c,lenOfUL);
	
	//接收返回的UL
	vector<uint32_t> buf;
    //alignas(32) std::array<uint32_t, 4000> buf = { { 0 } };
    unsigned long num;
	c2h.reoffset(off1);
    if(!ReadFile(c2h.h,&buf,(DWORD)4000,&num,NULL)){
        throw std::runtime_error("failed to read"+std::to_string(GetLastError()));
    }
    //------------------------------
   /* std::cout << "    Initiating H2C_" << 0 << " transfer of " << h2c_data.size() * sizeof(uint32_t) << " bytes...\n";
    h2c.write(h2c_data.data(), h2c_data.size() * sizeof(uint32_t));
    std::cout << "    Initiating C2H_" << 0 << " transfer of " << c2h_data.size() * sizeof(uint32_t) << " bytes...\n";
    c2h.read(c2h_data.data(), c2h_data.size() * sizeof(uint32_t));*/
    //------------------------------
	//创建文件来存储hui
	fstream file;
	file.open("C:\\Users\\xidian\\Desktop\\fhm\\hui.txt",ios::out); //以
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






