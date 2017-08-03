#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <stdint.h>

using namespace std;

#define DEBUG

#ifdef DEBUG
	#define DEBUGMSG(message) cout<<message
#else
	#define DEBUGMSG(message)
#endif

typedef uint16_t	word;

void error(int line, string msg){
	cerr << "Строка "<<line<<": "<<msg<< endl;
	exit(EXIT_FAILURE);
}

/* Опредление относительной/абсолютной адресации */
int relative(string &arg){
	int ret = 0;
	// если строка начинается c 0 (который раньше был [), то смотрим со следующего символа
	int offset = arg[0] == '0';
	if(arg[offset] == '+'){
		DEBUGMSG("-Относительный ув.\n");
		arg[offset] = '0';
		return 0x1400;
	}else if(arg[offset] == '-'){
		DEBUGMSG("-Относительный ум.\n");
		arg[offset] = '0';
		return 0x1000;
	}
	// если не относительная адресация, то возвращаем ноль
	return ret;
}

int defnotation(string arg){
	int ret;
		if(arg[arg.size()-1] == 'h') // если на конце стоит h
			ret = stoi(arg, nullptr, 16); // система счисления шеснадцатиричная
		else
			ret = stoi(arg, nullptr, 10); // иначе -- десятичная
	return ret;
}

int assembly(char *ifilename, char *ofilename, int mode){
	map <string, int> table{
		{"and",		0x0000}, // 0
		{"add",		0x2000}, // 1
		{"sca",		0x4000}, // 2
		{"inz",		0x6000}, // 3
		{"jmp",		0x8000}, // 4
		{"call",	0xA000}, // 5
		{"in",		0xC000}, // 6 (0)
		{"out",		0xD000}, // 6 (1)
		{"ext",		0xE000}, // 7 (0)
		{"low",		0xF000}, // 7 (1,0)
		{"high",	0xF800}, // 7 (1,1)
	};
	struct ext {
		char name[6];
		int code;
	};
	struct ext ext0[9]{
		{"nota", 0x800},
		{"inca", 0x400},
		{"notal", 0x200},
		{"shlc", 0x100},
		{"swap", 0x80},
		{"skc", 0x8},
		{"ei", 0x4},
		{"di", 0x2},
		{"halt", 0x1}
	};
	struct ext ext1[3]{
		{"nota", 0x400},
		{"add", 0x200},
		{"notal", 0x100},
	};
	map <string, int> labels;
	ifstream ifile(ifilename);
	if(!ifile.is_open())
		return 1;
	ofstream ofile(ofilename);
	if(!ofile.is_open())
		return 1;
	string rawline, instr, arg;
	int offset=0, lineCntr=1;
/* считываем первое слово из каждой строки
	если на конце двоеточие
	то добавляем её в карту со значением текущего смещения
 */
	while ( getline(ifile, rawline) ){
		if(!rawline.size()) // если строка пуста то просто берем следующую
			continue;
		stringstream line(rawline);
		line>>instr; // получаем первое слово
		if(instr[0] == ';'){ // если не комментарий
                        continue;
                }
		if(instr[0] == '"'){ // это строка данных, надо посчитать её длинну
			DEBUGMSG("-Строка @"<<offset<<":"<<offset+instr.size()-3<<"\n");
			offset+=instr.size()-3;
		}
		if(instr[instr.size()-1] != ':'){ // если не метка
			offset++;
			continue;
		}
		instr.resize(instr.size()-1);
		labels[instr]=offset;
		DEBUGMSG("-Метка "<<instr<<" @ "<<offset<<"\n");
	}
	offset = 0;
	ifile.clear();
	ifile.seekg(0, ios::beg); // переходим в начало файла
/* считываем строки и ассемблируем их */
	while ( getline(ifile, rawline) ){
		word instruction=0, opcode;
		if(!rawline.size()){ // если строка пуста
			lineCntr++;
			continue;
		}else if(rawline[0] == ';'){ // если она начинается с комментария
			lineCntr++;
			continue;
		}else if(rawline[0] == '#'){ // если она начинается с решетки, то это слово данных
			rawline[0]='0';
			rawline[1]='0'; // заместить решетку и пробел на нули
			instruction = defnotation(string(rawline));
		}else if(rawline[0] == '"'){ // если она начинается с ковычек, то это строка
			string data = rawline.substr( 1, rawline.size()-2 );
			lineCntr++;
			offset+=data.size(); // смещение следующей инструкции
			unsigned char zero = 0;
			// выводим строку в файл
			for(int i=0; i<data.size(); i++)
				ofile<<zero<<data[i];
			continue;
		}else{
			stringstream line(rawline);
			line>>instr; // получаем мнемонику инструкции
			DEBUGMSG("-Первое слово: "<<instr<<endl);
			// FIXME может быть только одна метка на строку
			if(instr[instr.size()-1] == ':'){ //если получили метку, считываем следующее слово
				line>>instr;
				if(line.tellg() == -1){// если больше на строке ничего нет кроме метки, переходим на след.
					lineCntr++;
					continue;
				}
			}
			//transform(instr.begin(), instr.end(), instr.begin(), ::tolower); // преобразование в нижний регистр
			if ( table.find(instr) == table.end() ) {
				error(lineCntr, "Неизвестная инструкция");
			} else {
				opcode = table[instr]; // инструкция нашлась, ищем оп.код
				DEBUGMSG("-Инстр("<<instr<<") ОпКод("<<opcode<<")\n");
				instruction |= opcode; // закидываем оп.код
				if(opcode<0xC000){ // если это основная инструкция и не ввод-вывод
					// определяем режим адресации
					if(line.tellg() == -1)
						error(lineCntr, "Не достаточно аргументов");
					line>>arg;
					if ( labels.find(arg) != labels.end() ) { // если строковый аргумент нашелся в карте меток
						int jumpTo = labels[arg]; // на какое смещение нам надо перейти
						int diff; // на сколько производить относительный переход
						DEBUGMSG("-Метка на "<<jumpTo<<endl);
						// TODO расшиирить вариативность джампа
						if(offset<jumpTo){
							diff = jumpTo - offset;
							instruction |= 0x1400 | diff; // относительный прямой переход +
						}else{
							diff = offset - jumpTo;
							instruction |= 0x1000 | diff; // относительный прямой переход -
						}
					}else{
						if(arg[0] == '['){ // косвенная адресация - поставить 4 бит
							DEBUGMSG("-Косвенная адр\n");
							instruction |= 0x800;
							arg[0]='0'; // заменяем открывающуюся скобку на цифру
							arg.resize(arg.size()-1); // сжимаем строку, исключая закр. скобку
						}else
							// прямая адресация - 4 бит и так в нуле
							DEBUGMSG("-Прямая адр\n");
						instruction |= relative(arg); //может быть это relative адресация
						try{
							instruction |= defnotation(arg); // определяем аргумент
						}catch(std::invalid_argument&){
							error(lineCntr, "Не число");
						}
					}
				}
				if(opcode == 0xC000 || opcode == 0xD000){ //ввод-вывод
					if(line.tellg() == -1)
						error(lineCntr, "Не достаточно аргументов");
					line>>arg;
					try{
						instruction |= relative(arg);
					}catch(std::invalid_argument&){
						error(lineCntr, "Не число");
					}
				}
				if(opcode == 0xE000){ // нулевая расширенная инструкция
					for(int i=0; i<9; i++)
						if(rawline.find(ext0[i].name) != string::npos)
							instruction |= ext0[i].code;
					// TODO получение численного arg (<8) на смещение
					// if(arg > 255) error(lineCntr, "Большое число");
					// instruction |= arg<<4
				}
				if(opcode == 0xF000 || opcode == 0xF800){ // первая расширенная инструкция
					for(int i=0; i<3; i++)
						if(rawline.find(ext1[i].name) != string::npos)
							instruction |= ext1[i].code;
					while(line.tellg() != -1) line>>arg; // последнее число из потока
					int temp = defnotation(arg);
					if(temp > 256)
						error(lineCntr, "Большое число");
					instruction |= temp;
				}
			}
		}
		if( mode ) // если режим отладочного вывода включен
			printf("%x\t%X:%X\t%i:%s\n", 
				offset++,
				((unsigned char *)&instruction)[1],
				((unsigned char *)&instruction)[0],
				lineCntr++,
				rawline.c_str()
			);
		// вывод в файл
		ofile<<((unsigned char *)&instruction)[0]<<((unsigned char *)&instruction)[1];
	}
	// закрыть файлы и выйти
	ifile.close();
	ofile.close();
	return 0;
}

int main(int argc, char **argv){
	// 0	   1	    2  3	4
	// asmmisc code.asm -o code.bin	-W
	int mode = 0;
	if( argv[4] != NULL )
		if( !string(argv[4]).compare("-W") ) // compare возвращает 0 если строки равны
			mode = 1;
	if ( assembly(argv[1], argv[3], mode) )
		cout<<"Ошибка сборщика!\n";
	else
		cout<<"Сборщик завершил работу нормально.\n";
	return 0;
}
