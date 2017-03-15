
#include <map>
#include <set>
#include <cstdlib>
#include<stdlib.h>
#include <time.h>
#include<fstream>
#include<vector>

#define TESTNUM (100000)
using namespace std;

void createTestData(const char* output)
{
	ofstream ofs(output);
	int keyT[TESTNUM];
	long valueT[TESTNUM];
	set<int>dupe;
	int total = 0;
	while (total<TESTNUM)
	{
		int mode = rand() & ((1 << 4) - 1);
		if (mode<1 && total>0)
		{
			int pos = rand() % total;
			while (keyT[pos] == -1)
			{
				pos = rand() % total;
			}
			dupe.erase(keyT[pos]);
			ofs << "2 " << keyT[pos] << endl;
			keyT[pos] = -1;
		}
		else if (mode<11 || total == 0)
		{
			int key = rand();
			long value = (long)rand() | (((long)rand()) << 32);
			if (dupe.count(key) == 0)
			{
				keyT[total] = key;
				valueT[total] = value;
				total++;
				dupe.insert(key);
				ofs << "1 " << key << " " << value << endl;
			}
		}
		else
		{
			int pos = rand() % total;
			while (keyT[pos] == -1) pos = rand() % total;
			long value = (long)rand() | (((long)rand()) << 32);
			ofs << "3 " << keyT[pos] << " " << value << endl;
			valueT[pos] = value;
		}
		int posT = rand() % total;
		while (keyT[posT] == -1) posT = rand() % total;
		ofs << "4 " << keyT[posT] << " " << valueT[posT] << endl;
	}
	ofs.close();
}
int main()
{
	srand((int)time(0));
	string fileName = "testData.txt";
	createTestData(fileName.c_str());
    return 0;
}
