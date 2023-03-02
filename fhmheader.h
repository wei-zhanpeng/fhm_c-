#include <vector>
using namespace std;

class Pair
{
public:
		int item = 0;
		long utility = 0;
};

class Element
{
public:
	int tid ;
	int iutils ;
	int rutils ;

	int is_exist ;

	Element(): tid(0),iutils(0),rutils(0),is_exist(0){}

	void set(int tid,int iutils,int rutils){
		this->tid = tid;
		this->iutils = iutils;
		this->rutils = rutils;
		this->is_exist=1;
	}
	void operator = (Element ele){
		this->tid = ele.tid;
		this->iutils = ele.iutils;
		this->rutils = ele.rutils;
	}
	
};


class UtilityList
{
public:
	int item ;
	int sumIutils ;
	int sumRutils ;

	int is_exist;
    int sizeofself = sizeof(UtilityList);

	vector<Element> elements;// = new Element();

	UtilityList():item(0),sumIutils(0),sumRutils(0) ,is_exist(0){}
    UtilityList(int a,int b,int c,int d):item(a),sumIutils(b),sumRutils(c) ,is_exist(d){}
    //使用深拷贝
    UtilityList(UtilityList &ul){
        this->item  = ul.item;
        this->sumIutils = ul.sumIutils;
        this->sumRutils = ul.sumRutils;
        this->is_exist = ul.is_exist;
        this->elements = new vector<Element> (ul.elements);
    }
    ~ UtilityList(){};
	
	void set(int item, int sumIutils, int sumRutils)
	{
		this->item = item;
		this->sumIutils = sumIutils;
		this->sumRutils = sumRutils;
		this->is_exist = 1;
	}

	void addElement(Element element)
	{
		this->sumIutils += element.iutils;
		this->sumRutils += element.rutils;
		this->is_exist = 1;
		elements.push_back(element);
	}

	bool operater == (int iteme){
		return ((this->item) == iteme);
	}

};
