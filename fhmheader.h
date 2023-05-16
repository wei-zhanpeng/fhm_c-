#ifndef FHMHEADER
#define FHMHEADER
#include <vector>
#include <iostream>
class Pair
{
public:
		int item = 0;
		int utility = 0;
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
	/*
	void operator = (Element ele){
		this->tid = ele.tid;
		this->iutils = ele.iutils;
		this->rutils = ele.rutils;
	}*/
	
};

struct ultopcie{
	int item;
	int sumIutils;
	int sumRutils;
	vector<Element> elements;
}ULstruct;


class UtilityList
{
public:
	int item ;
	int sumIutils ;
	int sumRutils ;
	vector<Element> elements;// = new Element();

	int is_exist;
    int sizeofself = 12;
	UtilityList():item(0),sumIutils(0),sumRutils(0) ,is_exist(0){}
    UtilityList(int a,int b,int c,int d):item(a),sumIutils(b),sumRutils(c) ,is_exist(d){}
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
		this->sizeofself += 12;
	}

};
//--------------------struct UL 接收返回的UL---------------//
struct ULItemset{
    int item ;
	int sumIutils ;
	int sumRutils ;
    vector<Element> elements;
};
#endif