#ifndef _BPLUSTREE_H_
#define _BPLUSTREE_H_

#include "MemoryHandler.h"
#include <stack>

#define BTORDER (4)


template<typename KeyType,typename ValueType>
class BPlusMap
{
public:
    BPlusMap(const char* indexFileName,const char* keyFileName,const char* dataFileName);
    ~BPlusMap();
    void insert(const KeyType& key,const ValueType& value);
    ValueType get(const KeyType& key);
    void remove(const KeyType& key);
    int update(const KeyType& key,const ValueType& value);
private:
    struct Node
    {
        int num;
	bool isLeaf;
	long long keyAddr[BTORDER];
	long long childAddr[BTORDER];
    };
    struct Record
    {
       Node* node;
       long long addr;
       int pos;
       Record(){};
       Record(Node* nn,long long address,int position)
           :node(nn),addr(address),pos(position){};
    };
    MemoryHandler<Node>* indexManager;
    MemoryHandler<ValueType>* dataManager;
    MemoryHandler<KeyType>*keyManager;
    long long keyBuffer[BTORDER+1];
    long long childBuffer[BTORDER+1];
    Node* root;
    Node memoryNode;
    BPlusMap();
    long long addNodeInMemory();
    int searchInNode(Node* currentNode,const KeyType& key,const int& mode);
    int insertInNode(Node* currentNode,long long  keyAddress,long long  childAddress,const int& pos);
    void splitNode(Node* currentNode,long long & keyAddress, long long & childAddress,int position);
    void splitRoot(Node* currentNode,long long & keyAddress,long long & childAddress,int position);
    void putInBuffer(Node* currentNode,long long  keyAddress,long long  childAddress, int position);
    void removeInNode(Node* currentNode,int position);
    int borrowFromSibling(Node* currentNode,long long & index,long long & keyAddress,long long & childAddress,int position);
    int combine(Node* currentNode,long long & index,long long & keyAddress,long long & childAddress,int position);
};

template<typename KeyType,typename ValueType>
ValueType BPlusMap<KeyType,ValueType>::get(const KeyType& key)
{
    long long address=PAGESIZE<<1-PAGEREST;
    int position,addrTemp;
    Node* currentNode=(Node*)(indexManager->getAddr(address));
    int num=currentNode->num;
    if (num==0)
    {
        indexManager->unMapAddr(address);
	throw string("BPLUSMAP EMPTY");
    }
    while (true)
    {
        if (currentNode->isLeaf)
            position=searchInNode(currentNode,key,2);
	else 
	    position=searchInNode(currentNode,key,0);
	if (currentNode->isLeaf)
	{
	    if (position==-1)
            {
	        indexManager->unMapAddr(address);
	        throw string("BPLUSMAP QUERY ERROR: KEY NOT FOUND!");
	    }
	    else
	    {
	        indexManager->unMapAddr(address);
	        return dataManager->getValue(currentNode->childAddr[position]);
	    }
	}
	addrTemp=address;
	if (currentNode->num==position)
            address=currentNode->childAddr[position-1];
	else 
	    address=currentNode->childAddr[position];
	indexManager->unMapAddr(addrTemp);
	currentNode=(Node*)(indexManager->getAddr(address));
    }
}

template<typename KeyType,typename ValueType>
int BPlusMap<KeyType,ValueType>::update(const KeyType& key,const ValueType& value)
{
    long long address=PAGESIZE<<1-PAGEREST;
    int position,addrTemp;
    ValueType valueTemp=value;
    Node* currentNode=(Node*)(indexManager->getAddr(address));
    int num=currentNode->num;
    if (num==0)
    {
        indexManager->unMapAddr(address);
	throw string("BPLUSMAP EMPTY");
    }
    while (true)
    {
        if (currentNode->isLeaf)
            position=searchInNode(currentNode,key,2);
	else 
	    position=searchInNode(currentNode,key,0);
	if (currentNode->isLeaf)
	{
	    if (position==-1)
            {
	        indexManager->unMapAddr(address);
	        throw string("BPLUSMAP UPDATE ERROR: KEY NOT FOUND!");
	    }
	    else
	    {
	        indexManager->unMapAddr(address);
	        dataManager->update(currentNode->childAddr[position],&valueTemp);
		return 0;
	    }
	}
	addrTemp=address;
	if (currentNode->num==position)
            address=currentNode->childAddr[position-1];
	else 
	    address=currentNode->childAddr[position];
	indexManager->unMapAddr(addrTemp);
	currentNode=(Node*)(indexManager->getAddr(address));
    }
    return -1;
}

template<typename KeyType,typename ValueType>
BPlusMap<KeyType,ValueType>::BPlusMap(const char* indexFileName,const char* keyFileName,const char* dataFileName)
{
    indexManager=new MemoryHandler<Node>(indexFileName);
    keyManager=new MemoryHandler<KeyType>(keyFileName);
    dataManager=new MemoryHandler<ValueType>(dataFileName);
    memoryNode.num=0;
    memoryNode.isLeaf=true;
    memset(memoryNode.keyAddr, 0, sizeof(long long) * (BTORDER));
    memset(memoryNode.childAddr, 0, sizeof(long long) * BTORDER);
    if (indexManager->getTotal()==1)
        addNodeInMemory();
}

template<typename KeyType,typename ValueType>
BPlusMap<KeyType,ValueType>::~BPlusMap()
{
    delete indexManager;
    delete keyManager;
    delete dataManager;
}

template<typename KeyType,typename ValueType>
long long BPlusMap<KeyType,ValueType>::addNodeInMemory()
{
    return indexManager->insert(&memoryNode);
}

template<typename KeyType,typename ValueType>
int BPlusMap<KeyType,ValueType>::searchInNode(Node* currentNode,const KeyType& key,const int& mode)
{
    int left=0;
    int right=currentNode->num;
    while (left<right)
    {
        int mid=(left+right)>>1;
	int check=keyManager->compare(currentNode->keyAddr[mid],key);
	if (check==0)
	{
	    if (mode==1) return -1;
	    return mid;
	}
	if (check==-1) left=mid+1;
	else right=mid;
    }
    if (mode==2) return -1;
    else return left;
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::putInBuffer(Node* currentNode,long long  keyAddress,long long  childAddress, int position)
{
    for(int i=0; i<position; i++){
        keyBuffer[i] = currentNode->keyAddr[i];
        childBuffer[i] = currentNode->childAddr[i];
    }
    keyBuffer[position] =  keyAddress;
    childBuffer[position] = childAddress;
    for(int i=position; i<currentNode->num; i++){
        keyBuffer[i+1] = currentNode->keyAddr[i];
        childBuffer[i+1] = currentNode->childAddr[i];
    }
}


template<typename KeyType,typename ValueType>
int BPlusMap<KeyType,ValueType>::insertInNode(Node* currentNode,long long  keyAddress,long long  childAddress,const int& pos)
{
    for(int i=currentNode->num-1; i>=pos; i--)
    {
        currentNode->keyAddr[i+1] = currentNode->keyAddr[i];
        currentNode->childAddr[i+1] = currentNode->childAddr[i];
    }
    currentNode->num++;
    currentNode->keyAddr[pos] = keyAddress;
    currentNode->childAddr[pos] = childAddress;
    return 0;
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::insert(const KeyType& key,const ValueType& value)
{
    int position;
    bool isMax=false;
    long long keyAddress,childAddress,keyAddressTemp;
    KeyType keyTemp=key;
    ValueType valueTemp=value;
    int address=(PAGESIZE<<1)-PAGEREST;
    stack<Record>record;
    Node* currentNode=(Node*)(indexManager->getAddr(address));
    while (true)
    {
        position=searchInNode(currentNode,key,1);
	if (position==-1) 
	{
	    while (record.size()) 
	    {
	        indexManager->unMapAddr(record.top().addr);
	        record.pop();
            }
	    return;
	}
	record.push(Record(currentNode,address,position));
	if (currentNode->isLeaf)
        {
            if (position==currentNode->num) isMax=true;
            break;
        }
	if (currentNode->num==position) address=currentNode->childAddr[position-1];
	else address=currentNode->childAddr[position];
	currentNode=(Node*)(indexManager->getAddr(currentNode->childAddr[position]));
    }
    keyAddress=keyManager->insert(&keyTemp);
    childAddress=dataManager->insert(&valueTemp);
    keyAddressTemp=keyAddress;
    while (record.size())
    {
        position=record.top().pos;
	address=record.top().addr;
	currentNode=record.top().node;
	record.pop();
	if (isMax) currentNode->keyAddr[position]=keyAddressTemp;
	if (currentNode->num!=BTORDER)
	{
	    insertInNode(currentNode,keyAddress,childAddress,position);
	    indexManager->unMapAddr(address);
	    break;
	}
	else if (record.size())
	{
	    splitNode(currentNode,keyAddress,childAddress,position);
	}
	else 
	{
	    splitRoot(currentNode,keyAddress,childAddress,position);
	}
    }
    while (record.size())
    {
        if (isMax) record.top().node->keyAddr[record.top().pos]=keyAddressTemp;
        indexManager->unMapAddr(record.top().addr);
	record.pop();
    }
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::splitNode(Node* currentNode,long long & keyAddress,long long & childAddress,int position)
{
    long long sibling=addNodeInMemory();
    Node* sib=(Node*)(indexManager->getAddr(sibling));
    putInBuffer(currentNode,keyAddress,childAddress,position);
    currentNode->num=BTORDER>>1;
    sib->num=BTORDER-currentNode->num;
    sib->isLeaf=currentNode->isLeaf;
    for (int i=0;i<currentNode->num;i++)
    {
        currentNode->keyAddr[i]=keyBuffer[i];
	currentNode->childAddr[i]=childBuffer[i];
    }
    for (int i=currentNode->num;i<BTORDER+1;i++)
    {
        sib->keyAddr[i-currentNode->num]=keyBuffer[i];
	sib->childAddr[i-currentNode->num]=childBuffer[i];
    }
    keyAddress=currentNode->keyAddr[currentNode->num-1];
    childAddress=sibling;
    indexManager->unMapAddr(sibling);
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::splitRoot(Node* currentNode,long long & keyAddress,long long & childAddress,int position)
{
    long long leftAddress,rightAddress;
    leftAddress=addNodeInMemory();
    rightAddress=addNodeInMemory();
    Node* left=(Node*)(indexManager->getAddr(leftAddress));
    Node* right=(Node*)(indexManager->getAddr(rightAddress));
    putInBuffer(currentNode,keyAddress,childAddress,position);
    left->num=BTORDER>>1;
    right->num=BTORDER-left->num;
    left->isLeaf=currentNode->isLeaf;
    right->isLeaf=currentNode->isLeaf;
    for (int i=0;i<left->num;i++)
    {
        left->keyAddr[i]=keyBuffer[i];
	left->childAddr[i]=childBuffer[i];
    }
    for (int i=left->num;i<=BTORDER;i++)
    {
        right->keyAddr[i-left->num]=keyBuffer[i];
	right->childAddr[i-left->num]=childBuffer[i];
    }
    currentNode->num=2;
    currentNode->keyAddr[0]=left->keyAddr[left->num-1];
    currentNode->keyAddr[1]=right->keyAddr[right->num-1];
    currentNode->childAddr[0]=leftAddress;
    currentNode->childAddr[1]=rightAddress;
    currentNode->isLeaf=false;
    indexManager->unMapAddr(leftAddress);
    indexManager->unMapAddr(rightAddress);
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::removeInNode(Node* currentNode,int position)
{
    for (int i=position;i<currentNode->num-1;i++)
    {
        currentNode->keyAddr[i]=currentNode->keyAddr[i+1];
	currentNode->childAddr[i]=currentNode->childAddr[i+1];
    }
    currentNode->num--;
}

template<typename KeyType,typename ValueType>
void BPlusMap<KeyType,ValueType>::remove(const KeyType& key)
{
    KeyType keyTemp=key;
    long long address,keyAddressTemp=0;
    long long keyAddress,childAddress,indexTemp;
    int position;
    bool isMax=false;
    bool halfEmpty=false;
    address=PAGESIZE<<1-PAGEREST;
    stack<Record>record;
    Node* currentNode=(Node*)(indexManager->getAddr(address));
    while (true)
    {
    	if (currentNode->isLeaf)
            position=searchInNode(currentNode,key,2);
	else
	    position=searchInNode(currentNode,key,0);
	if (position==-1) 
	{
	    while (record.size()) 
	    {
	        indexManager->unMapAddr(record.top().addr);
	        record.pop();
            }
	    return;
	}
	record.push(Record(currentNode,address,position));
	if (currentNode->isLeaf)
	{
	    if (position==currentNode->num-1)
	        isMax=true;
	    removeInNode(currentNode,position);
	    break;
	}
	address=currentNode->childAddr[position];
	currentNode=(Node*)(indexManager->getAddr(address));
    }
    keyManager->remove(currentNode->keyAddr[position]);
    dataManager->remove(currentNode->childAddr[position]);
    if (currentNode->num>1) keyAddressTemp=currentNode->keyAddr[currentNode->num-1];
    while (record.size())
    {
        currentNode=record.top().node;
	address=record.top().addr;
	position=record.top().pos;
	if (isMax) currentNode->keyAddr[position]=keyAddressTemp;
	if (halfEmpty)
	{
	    int check=borrowFromSibling(currentNode,indexTemp,keyAddress,childAddress,position);
	    if (check==-1)
	    {
	        int flag=combine(currentNode,indexTemp,keyAddress,childAddress,position);
		if (flag==-1)
		{
		    break;
		}
		else if (flag==1)
		{
		    currentNode->keyAddr[position-1]=indexTemp;
		    removeInNode(currentNode,position);
		}
		else
		{
		    currentNode->keyAddr[position]=indexTemp;
		    removeInNode(currentNode,position+1);
		}
	    }
	    else if (check==1)
	    {
	        currentNode->keyAddr[position-1]=indexTemp;
	        Node* child=(Node*)(indexManager->getAddr(currentNode->childAddr[position]));
	        insertInNode(child,keyAddress,childAddress,0);
		indexManager->unMapAddr(currentNode->childAddr[position]);
	    }
	    else 
	    {
	        currentNode->keyAddr[position]=indexTemp;
		Node* child=(Node*)(indexManager->getAddr(currentNode->childAddr[position]));
	        insertInNode(child,keyAddress,childAddress,child->num-1);
		indexManager->unMapAddr(currentNode->childAddr[position]);
	    }
	}
	halfEmpty=currentNode->num < ((BTORDER+1)>>1);
	indexManager->unMapAddr(address);
	record.pop();
    }
}

template<typename KeyType,typename ValueType>
int BPlusMap<KeyType,ValueType>::
borrowFromSibling(Node* currentNode,long long & index,long long & keyAddress,long long & childAddress,int position)
{
    long long currentAddress;
    int leftNum,rightNum;
    Node* left=NULL,*right=NULL;
    if (currentNode->num-1<(BTORDER+1)>>1)
    {
	return -1;
    }
    if (position>0) 
    { 
        left=(Node*)(indexManager->getAddr(currentNode->childAddr[position-1]));
	leftNum=left->num; 
    }
    else leftNum=-1;
    if (currentNode->num>position+1) 
    {
        right=(Node*)(indexManager->getAddr(currentNode->childAddr[position+1]));
	rightNum=right->num;
    }
    else rightNum=-1;
    if (leftNum+rightNum==-2) return -1;
    if (leftNum==-1 && rightNum>(BTORDER+1)>>1)
    {
        currentAddress=currentNode->childAddr[position-1];
        indexManager->unMapAddr(currentNode->childAddr[position-1]);
	left=NULL;
    }
    else if (rightNum==-1 && leftNum>(BTORDER+1)>>1)
    {
        currentAddress=currentNode->childAddr[position+1];
        indexManager->unMapAddr(currentNode->childAddr[position+1]);
	right=NULL;
    }
    else if (leftNum>(BTORDER+1)>>1 && rightNum>(BTORDER+1)>>1)
    {
        if (leftNum>=rightNum)
	{
            currentAddress=currentNode->childAddr[position+1];
	    indexManager->unMapAddr(currentNode->childAddr[position+1]);
	    right=NULL;
	}
	else 
	{
	    currentAddress=currentNode->childAddr[position-1];
	    indexManager->unMapAddr(currentNode->childAddr[position-1]);
	    left=NULL;
	}
    }
    else 
    {
        indexManager->unMapAddr(currentNode->childAddr[position+1]);
        indexManager->unMapAddr(currentNode->childAddr[position-1]);
        return -1;
    }
    if (left)
    {
        keyAddress=left->keyAddr[left->num-1];
	childAddress=left->childAddr[left->num-1];
        index=left->keyAddr[left->num-2];
	removeInNode(left,left->num-1);
	indexManager->unMapAddr(currentAddress);
	return 1;
    }
    if (right)
    {
        keyAddress=right->keyAddr[0];
	childAddress=right->childAddr[0];
	index=right->keyAddr[0];
	removeInNode(right,0);
        indexManager->unMapAddr(currentAddress);
	return 2;
    }
    return -1;
}

template<typename KeyType,typename ValueType>
int BPlusMap<KeyType,ValueType>::
combine(Node* currentNode,long long & index,long long & keyAddress,long long & childAddress,int position)
{
    Node* child=(Node*)(indexManager->getAddr(currentNode->childAddr[position]));
    if (currentNode->num<2)
    {
	return -1;
    }
    Node* left=NULL;
    Node* right=NULL;
    if (position>0)
        left=(Node*)(indexManager->getAddr(currentNode->childAddr[position-1]));
    else if (currentNode->num-1-position>0)
        right=(Node*)(indexManager->getAddr(currentNode->childAddr[position+1]));
    else 
    {
	return -1;
    }
    if (left)
    {
        if (right) 
	    indexManager->unMapAddr(currentNode->childAddr[position+1]);
	index=child->keyAddr[child->num-1];
	for (int i=0;i<child->num;i++)
	{
	    left->keyAddr[left->num+i]=child->keyAddr[i];
	    left->childAddr[left->num+i]=child->childAddr[i];
	}
	left->num+=child->num;
	indexManager->unMapAddr(currentNode->childAddr[position]);
	indexManager->unMapAddr(currentNode->childAddr[position-1]);
	indexManager->remove(currentNode->childAddr[position]);
	return 1;
    }
    else if (right)
    {
        if (left) indexManager->unMapAddr(currentNode->childAddr[position-1]);
	index=right->keyAddr[right->num-1];
	for (int i=0;i<right->num;i++)
	{
	    child->keyAddr[child->num+i]=right->keyAddr[i];
	    child->childAddr[child->num+i]=right->childAddr[i];
	}
	child->num+=right->num;
	indexManager->unMapAddr(currentNode->childAddr[position]);
	indexManager->unMapAddr(currentNode->childAddr[position+1]);
	indexManager->remove(currentNode->childAddr[position+1]);
	return 2;
    }
    else
    {
        return -1;
    }
}


#endif
