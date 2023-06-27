#include <ctime>
#include <map>
#include <list>
#include <iostream>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <list>
#include <algorithm>
//pcie driver
#include "pcie_api.h"
#include "xdma_public.h"
//fhm 类定义文件
using namespace std;
#include "fhmheader.h"
#include <chrono>
//加载静态库
#pragma comment(lib, "setupapi")
//
#define FILE_NAME "db.txt"
#define ENABLE_LA_PRUNE true
//
clock_t  Begin, End;
double duration;
int minUtility = 20000;
int huiCount = 0;
unordered_map<int,int> mapItemToTwu;
unordered_map<int,unordered_map<int,int>> mapFMAP;
unordered_map<int, int> mapItemToTwuList;  //存储为<item,twu>格式
static vector<UtilityList> listOfUtilityLists;//只存储ulist对应的item,其他不保存在其中
unordered_map<int,UtilityList> mapItemToUtilityList;
int candidateCount = 0;
int prefix[200];
int itemset[200];
int offset[10];
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
//construct depfirst
static UtilityList construct(UtilityList &X,UtilityList &Y){

	UtilityList pxyUL;
	pxyUL.item = Y.item;
	UtilityList &px = X;
	UtilityList &py = mapItemToUtilityList[Y.item];
	
	for(Element ex : px.elements){
		Element ey ;
		findElementWithTID(py,ex.tid,ey);
		if(ey.is_exist==1){
			Element eXY ;
			eXY.set(ex.tid,ex.iutils+ey.iutils,ey.rutils);
			pxyUL.addElement(eXY);
			pxyUL.is_exist=1;
		}		
	}
	return pxyUL;
}
//fhm-fit hardware
//ULs为所有一项集的效用列表，UL_nitemset为一个n项集的效用列表，itemset存储n项集的所有item，itemsetlen存储itemset长度
//itemset[],itemsetlen=0,ULs,ULs,offset[]={0},file
static void fhm_fithd(int itemset[],int itemsetlen,vector<UtilityList> &UL_nitemset,vector<UtilityList> &ULs,int offset[],fstream &file){
	//suffix_idx标注ULs中对应位置的item的坐标，idx用来标注offset的长度
	int prefix_idx=0;
	//vector<int> off;
	int off[2000];
	//-------------------------------intr
	/*auto event_loop = [&](unsigned event_id) {
        std::cout << ("Waiting on event_" + std::to_string(event_id) + "...\n");
        while (true) {

                const auto event_dev_path = device_paths[0] + "\\event_" + std::to_string(event_id);
                device_file user_event(event_dev_path, GENERIC_READ);
                auto val = user_event.read_intr<uint8_t>(0); // this blocks in driver until event is triggered
                if (val == 1) {
                    std::cout << ("event_" + std::to_string(event_id) + " received!\n");
                } // else timed out, so try again
        }
    };
    std::thread event_threads[] = { std::thread(event_loop, 0)};

    for (auto& t : event_,threads) {
        t.join();
    }  */
	//-------------------------------
	for(vector<UtilityList>::iterator it1=UL_nitemset.begin();it1!=UL_nitemset.end();it1++){
		UtilityList X;
		if(itemsetlen==0) {
			X = mapItemToUtilityList[it1->item];
		}
		else X = *it1;
		
		vector<UtilityList> exULs;
		//判断是否为hui
		if(X.sumIutils >= minUtility){
			//writeout;
			writeOut(itemset,itemsetlen,X.item,X.sumIutils,file);
		}
		if(X.sumIutils+X.sumRutils >= minUtility){
			//用来索引off数组，其中存储着itemset最后一个item对应的位置
			//idx是索引off的长度，loc+offset[suffix——len]代表着该itemset的最后一个item在ULs中的位置
			int loc = 0,idx=0;
			//存在潜在的hui,则进行探索
			UtilityList Y;
			for(vector<UtilityList>::iterator it2=ULs.begin();it2!=ULs.end();it2++){
				loc = loc + 1;
				//这里可能存在it2超过end位置，产生错误
				//探索频繁2itemset时，需要加上prefix_idx保证是从上层循环的下一个item开始循环的
				if(itemsetlen==0) {
					if(it2+prefix_idx+1==ULs.end()) break;
					 Y= *(it2+prefix_idx+1);
				}
				//探索n>=3itemset时，需要加上n-1itemset的最后一个item的位置
				else {
					if(it2+offset[prefix_idx]+1==ULs.end()) break;
					Y = *(it2+offset[prefix_idx]+1);
				}
				Y = mapItemToUtilityList[Y.item];
				//直接construct，然后判断是否是hui或存在潜在的hui
				UtilityList temp = construct(X,Y);
				if((temp.is_exist==1) && (temp.sumIutils>=minUtility || temp.sumIutils+temp.sumRutils>=minUtility)){
					exULs.push_back(temp);
					//合成2itemset时，偏移需要加prefix_idx，才能让下一个fhm中内层循环的UL指向对应位置
					/*if(itemsetlen ==0) off.push_back(loc+prefix_idx);
					else off.push_back(loc+offset[prefix_idx]);*/
					//off[idx++] = loc+offset[prefix_idx];
					if(itemsetlen ==0) off[idx++] = loc+prefix_idx;
					else off[idx++] = loc+offset[prefix_idx];
				}
			}
		}
		prefix_idx++;
		itemset[itemsetlen] = X.item;
		fhm_fithd(itemset,itemsetlen+1,exULs,ULs,off,file);
	}
} 
//fhm
//ULs里面只存储了升序排列的item，并没有存取具体的Utilitylist，因此需要在mapitemtoutitlitylist中取得
/*static void fhm(int prefix[],int prefixlength,UtilityList &pUL,vector<UtilityList> &ULs,int minUtility,fstream &file){
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
*/
int main()
{
		//init_platform();
		//time cal
		auto begin = std::chrono::high_resolution_clock::now();
		//
		int prefixlength = 0;
		ifstream fin ;
		fin.open("C:\\Users\\xidian\\Desktop\\fhm\\retail_utility.txt",ios::in);
		if(!fin.is_open()) { cout << "error" <<endl; return 0 ;} 
        //------------------------------------
		//pcie device 查找
/* 	 	const auto device_paths = get_device_paths(GUID_DEVINTERFACE_XDMA);
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
		device_file bypass(device_paths[0] + "\\bypass" , GENERIC_READ);
        if (h2c.h == INVALID_HANDLE_VALUE || c2h.h == INVALID_HANDLE_VALUE) {
            std::cout << "Could not find h2c_" << 0 << " and/or c2h_" << 0 << "\n";
        } else {
            std::cout << "Found h2c_" << 0 << " and c2h_" << 0 << ":\n";
        }  */

		/*auto event_loop = [&](unsigned event_id) {
            std::cout << ("Waiting on event_" + std::to_string(event_id) + "...\n");
            while (true) {

                    const auto event_dev_path = device_paths[0] + "\\event_" + std::to_string(event_id);
                    device_file user_event(event_dev_path, GENERIC_READ);
                    auto val = user_event.read_intr<uint8_t>(0); // this blocks in driver until event is triggered
                    if (val == 1) {
                        std::cout << ("event_" + std::to_string(event_id) + " received!\n");
                    } // else timed out, so try again
            }
        };
    	std::thread event_threads[] = { std::thread(event_loop, 0)};*/

        /* for (auto& t : event_threads) {
            t.join();
        } */
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
				//怎么将两个ulist存取为一个地址空间？
				ulist.item = it->first;
				listOfUtilityLists.push_back(ulist);
				mapItemToUtilityList.insert({it->first,ulist});
				//listOfUtilityLists.push_back(ulist);	
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
				UtilityList &itemUl = mapItemToUtilityList[pair.item];
				itemUl.addElement(element);
				
			}
			//每行读取完成tid++
			tid++;
			revisedTransaction.clear();
		}while(!fin.eof());
		// close file
		fin.close();

	
	UtilityList pUL;
	//---------------------------------------
	unsigned long lenOfUL=0;
	unsigned long ofs;
	unsigned long off1=0xc0000000;
	int data = 0xffff;
	auto uli = mapItemToUtilityList.begin();

/* 	//UL2FPGA(uli->second,offset,h2c,lenOfUL);
	h2c.reoffset(off1);
	WriteFile(h2c.h,&data,(DWORD)4,&ofs,NULL);
	//接收返回的UL
	vector<uint32_t> buf,buf1;
    //alignas(32) std::array<uint32_t, 4000> buf = { { 0 } };
    unsigned long num;
	c2h.reoffset(off1);
    if(!ReadFile(c2h.h,&buf,(DWORD)12,&num,NULL)){
        throw std::runtime_error("failed to read"+std::to_string(GetLastError()));
    } 
	bypass.reoffset(off1);
	if(!ReadFile(bypass.h,&buf1,(DWORD)12,&num,NULL)){
        throw std::runtime_error("failed to read"+std::to_string(GetLastError()));
    } */
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
	//time end
	auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    printf("without fhm Time measured: %.3f seconds.\n", elapsed.count() * 1e-9);
	
	//fhm(prefix,prefixlength,pUL,listOfUtilityLists,minUtility,file);
	//vector<int> offset ={0};
	fhm_fithd(itemset,0,listOfUtilityLists,listOfUtilityLists,offset,file);
	file.close();
	//fhm end
	end = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    printf("full Time measured: %.3f seconds.\n", elapsed.count() * 1e-9);
	
	return 0;
}






