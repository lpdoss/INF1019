	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>

#define SIZE_AGE 8

/******* Condiçoes de Retorno ************************/

typedef enum {
	 CondRetOK = 1,
	
	CondRetError = 2,
	
	CondRetPageTableExists = 3,
	
	CondRetFrameTableExists =4,
	
	CondRetMemoryIsFull=5,
	
	CondRetMemoryIsEmpty=6,
	
	CondRetNotInMemory=7,
	
	CondRetDirty=8
	
	
} tpCondRet;



/******* Definiçao das estruturas de dados *********/


/*********************************************************************
     Optei por utilizar uma unica estrutura para cada pagina que compoe
     a tabela de paginas e cada quadro que compoe a memoria. As funçoes
     de cada algoritmo e a funçao main apenas acessam aqueles campos
     relacionados ao algoritmo escolhido via parametro.
     
     Os campos que possuem descriçao da organizaçao dos seus bits
     obedecem o padrao de apresentar mais acima os bits mais a esquerda.
*********************************************************************/

typedef  struct page{
unsigned int pageInfo;
 /**************************************
		18 bits - Inuteis
		3 bits - ProteÃ§ao ( R W E) 
		1 bit - Presente / Ausente
		10 bits - Quadro de pagina 
	(Maior valor permitido de memoria
	real dividido pelo menor tam de
	pagina)
 
 			18 - 3 - 1 - 10
**************************************/ 
} tpPage;


typedef struct pageframe{
	unsigned char ControlBits;
/*************************************
		2 bits - Lixo
		1 bit - Referenciada 
		1 bit - Modificada
*************************************/
	int TimeofLastAcess;
	unsigned char Age; 
/*************************************
	Acessado apenas pelo NRU. 
	Preve um janela de 8 ticks do relogio
*************************************/
} tpPageFrame;


typedef struct SEGlist{
	int time;
	int pageNumber;
/*************************************
	Armazena apenas o numero de pagina
	para garantir que acesso ao quadro
	se de atraves das funçoes que 
	acessam a tabela de paginas.
*************************************/
	struct SEGlist *ptAnterior;
	struct SEGlist *ptProximo;
} tpSEGList;


/************** Protótipo de funções ***********************************/

tpCondRet createFrameTable(int numPageFrame);
int CountEmptyFrames(int numPageFrame);
int FindEmptyFrame(int numPageFrame);
tpCondRet EvictPageFromMemory(int pageNumber);
tpCondRet InsertPageToMemory(int pageNumber, int numFrame, char rw);
void SetRBitZero(int numPageFrame);
tpCondRet setTime(int pageNumber, int time);
tpCondRet TurnOnBitR(int pageNumber);
tpCondRet TurnOnBitM(int pageNumber);

tpCondRet createPageTable(int numPage);
int GetFrame(int pageNumber);
int CheckIfPresent(int pageNumber);
void TurnOnWrite(int pageNumber);
void TurnOnRead(int pageNumber);
void TurnOnPA(int pageNumber);

tpCondRet NRU(int numPage, int numFrame, int pageNumber, char rw);

tpCondRet LRU(int numPage, int numFrame, int pageNumber, char rw);
void LRUAge(int numPageFrame);
int LRUCompareAge(unsigned char currAge, unsigned char bestAge);

tpCondRet SEG(int numPage, int numFrame, int pageNumber, char rw, int time);
tpCondRet SEGInsert(int pageNumber, int time);
tpCondRet SEGRemove(int pageNumber);
tpCondRet SEGMoveToLast(int pageNumber, int time);


int tonumber(char str[]);
int power(int exp);
void GenerateReport(char *argv[], int contPageFault, int contDirty);
void housekeeping(int numPage, int numPageFrame);
void ShowMemory(int time, char *subst_alg,char debug[], char timeInterval[], int numPageFrame);


/**********************************************************************/

/************** Variaveis Globais **************************************/

tpPage **PageTable = NULL;
tpPageFrame **FrameTable = NULL;
tpSEGList *SEGList = NULL;
int contEvict =0, contInsert=0;
/***********************************************************************/


int main(int argc, char *argv[]){
	char *subst_alg= NULL;
	char *log_path= NULL;
	int size_Page=0,size_RealMem=0, numPageFrame=0, numPage=0, bits_offset=0, time=0;
	int contPageFault=0, contDirty=0, page=0, isDirty =0;
	unsigned int addr = 0;
	char rw = '\0';	
	FILE *fdLog = NULL;
	tpCondRet CondRet = CondRetOK;
	

	
	/* Inicializa valores */
	subst_alg		= argv[1];
	log_path		= argv[2];
	size_Page 		= tonumber(argv[3]);
	size_RealMem= tonumber(argv[4]);


	/* Caso tamanho seja recebido em MB converte para KB */
	if (size_RealMem <= 16)
		size_RealMem *= 1024;

	if(size_Page < 8 || size_Page > 32){
		printf("Tamanho de pagina invalido\n\n");
		return -1;
	}
	if(size_RealMem < 128 || size_RealMem > 16384){
		printf("Tamanho de memoria fisica invalido\n\n");
		return -1;
	}
	
	numPageFrame = size_RealMem / size_Page;

	/* Calcula numero de bits para offset. 
	   Em bits_offset bits, consigo representar 2^bits_offset valores = sizePage (em bytes).
	   Variando de 0 à (2^bits_offset - 1).
	*/
	size_Page *= 1024;
	while (size_Page != 1){
		size_Page = size_Page /2;
		bits_offset++;
	}
	
	numPage = (power(32-bits_offset)) -1;

	CondRet = createPageTable(numPage);
	if(CondRet != CondRetOK){
		printf("Erro ao alocar memoria\n\n");
		return -2;
	}
	CondRet = createFrameTable(numPageFrame);
	if(CondRet != CondRetOK){
		printf("Erro ao alocar memoria\n\n");
		return -2;
	}
	
	fdLog = fopen(log_path, "r");
	
	CondRet = CondRetOK;
	/* Fim da inicializaÃ§ao */
	/* Executa leitura de acessos a memoria */

	if(argv[5] != NULL && argv[6] != NULL){
		ShowMemory(time, subst_alg, argv[5], argv[6], numPageFrame);
		getchar();
		system("clear");
	}

	while (fscanf(fdLog, " %x %c ", &addr, &rw) > 0){
		time++;
		page = addr >> bits_offset;
		
		/*Trata Acesso */
		if (CheckIfPresent(page)){
			
			/* Se for escrita, modifico permissões e ligo o bit que informa modificação */
			if(rw == 'W'){
				TurnOnWrite(page);
				CondRet = TurnOnBitM(page);
				if(CondRet != CondRetOK){
					printf("Erro ao tentar alterar bit que informa modificação na página\n\n");
					return -3;
				}
			}
			
			CondRet = TurnOnBitR(page);
			if(CondRet != CondRetOK){
					printf("Erro ao tentar alterar bit referencia\n\n");
					return -3;
			}
			if (strcmp(subst_alg, "SEG") == 0){
				CondRet = SEGMoveToLast(page, time);
				if(CondRet != CondRetOK){
					printf("Erro ao mover na LISTA SEG, %d\n\n", CondRet);
					return -4;
				}
			}

		}
		else{
			contPageFault++;
			
			if (CountEmptyFrames(numPageFrame) == 0){
			
				
				/* Algoritmos de substituição garantem a mudança dos bits permissão, presente/ausente e controle */
				if( strcmp(subst_alg, "LRU") == 0)
					CondRet = LRU(numPage, numPageFrame, page, rw);
				else if( strcmp(subst_alg, "NRU") == 0)
					CondRet = NRU(numPage, numPageFrame, page, rw);
				else 
					CondRet = SEG(numPage, numPageFrame,page, rw, time);

				if(CondRet != CondRetOK && CondRet != CondRetDirty){
					printf("Erro ao executar substituição de pagina\n\n");
					return -5;
				if(CondRet == CondRetDirty)
					isDirty = 1;
				}
			}
			else{
				/* Insere garante a mudança dos bits permissão, presente/ausente e controle */
				CondRet = InsertPageToMemory(page, numPageFrame, rw);
				if(CondRet != CondRetOK && CondRet != CondRetDirty){
					printf("Erro ao tentar inserir pagina na memoria\n\n");
					return -6;
				}

				/*esta desligado, para mostrar apenas as paginas que foram substituidas */
				if(CondRet == CondRetDirty)
					isDirty = 0;

				if (strcmp(subst_alg, "SEG") == 0){
					CondRet = SEGInsert(page, time);
					if(CondRet != CondRetOK){
						printf("Erro ao inserir na LISTA SEG\n\n");
						return -4;
					}
				}
			}

		}
		
		/* Conclui e prepara para proxima iteraçao */
		if (strcmp(subst_alg, "LRU") == 0)
			LRUAge(numPageFrame);
		if ((strcmp(subst_alg, "NRU") == 0))
			SetRBitZero(numPageFrame);
		if (isDirty == 1)
			contDirty++;

		/* Será feito em todos os casos */
		CondRet = setTime(page, time);
		if(CondRet != CondRetOK){
			printf("Erro ao atualizar tempo de acesso da pagina\n\n");
			return -3;
		}
		if (argv[5] != NULL && argv[6] != NULL && (time%(tonumber(argv[6]))) == 0){
			ShowMemory(time, subst_alg, argv[5], argv[6], numPageFrame);
			getchar();
			system("clear");
		}
		isDirty = 0;
	}

	GenerateReport(argv, contPageFault, contDirty);
	
	fclose(fdLog);
	housekeeping(numPage, numPageFrame);
	return 0;
}



/****** FunÃ§oes de acesso a quadros de pagina *******/

tpCondRet createFrameTable(int numPageFrame){
	int i;
	if(FrameTable != NULL)
		return CondRetFrameTableExists;
	
	FrameTable = (tpPageFrame**) malloc(sizeof(tpPageFrame*)*numPageFrame);
	if(FrameTable == NULL)
		return CondRetError;

	for(i=0; i<numPageFrame;i++){
		FrameTable[i] = (tpPageFrame*) malloc(sizeof(tpPageFrame));
		if(FrameTable[i] == NULL)
			return CondRetError;
	}

	for(i=0; i<numPageFrame;i++){
		FrameTable[i]->ControlBits = 0x00;
		FrameTable[i]->Age = 0x00;
		FrameTable[i]->TimeofLastAcess = 0;
	}
	
	return CondRetOK;
}

int CountEmptyFrames(int numPageFrame){
	int frame =0, i;
	for( i=0; i<numPageFrame; i++){
		if(FrameTable[i]->TimeofLastAcess == 0)
			frame++;
	}
	return frame;
}

int FindEmptyFrame(int numPageFrame){
	int frame = -1, i;
	for( i=0; i<numPageFrame; i++){
		if(FrameTable[i]->TimeofLastAcess == 0){
			frame = i;
			break;
		}
	}
	return frame;
	
}

tpCondRet EvictPageFromMemory(int pageNumber){
	 int frame=0;
	 contEvict++;
	if(CheckIfPresent(pageNumber) == 0)
		return CondRetNotInMemory;
		
	frame = GetFrame(pageNumber);
	
	/* Limpa quadro de página */
	FrameTable[frame]->ControlBits = 0x00;
	FrameTable[frame]->TimeofLastAcess = 0;
	FrameTable[frame]->Age &= 0x00;
	
	/* Limpa entrada na tabela de paginas. Mantém apenas bits de proteção */
	PageTable[pageNumber]->pageInfo &= 0x00003800;
	
	return CondRetOK;
}

tpCondRet InsertPageToMemory(int pageNumber, int numFrame, char rw){
	int FrameNumber=0;
	tpCondRet CondRet=CondRetOK;
	contInsert++;
	FrameNumber = FindEmptyFrame(numFrame);
	if(FrameNumber == -1)
		return CondRetMemoryIsFull;
	
	/* Insere info na página da tabela */
	PageTable[pageNumber]->pageInfo = FrameNumber | (PageTable[pageNumber]->pageInfo & 0x00003C00);
	TurnOnPA(pageNumber);
	TurnOnRead(pageNumber);
	if (rw == 'W')
		TurnOnWrite(pageNumber);


	/* Insere info no quadro de pagina */
	CondRet = TurnOnBitR(pageNumber);
	if(CondRet != CondRetOK)
		return CondRet;
	if(rw == 'W'){
		CondRet = TurnOnBitM(pageNumber);
		if(CondRet != CondRetOK)
			return CondRet;
		else 
			return CondRetDirty;
	}
	/************************************************************ 
	   TimeofLastAcess e Age são modificados na função principal,
	   uma vez que precisam ser realizados também para páginas 
	   que já estão na memória.   
	************************************************************/
	return CondRetOK;	
}


void SetRBitZero(int numPageFrame){
	int i;
	for(i=0; i<numPageFrame; i++)
		FrameTable[i]->ControlBits &= 0x01;
	/*Zera tudo exceto bit M */
}

tpCondRet setTime(int pageNumber, int time){
	int frame;
	frame = GetFrame(pageNumber);
	if(frame == -1)
		return CondRetError;
	FrameTable[frame]->TimeofLastAcess = time;
	return CondRetOK;
}

tpCondRet TurnOnBitR(int pageNumber){
	int frame;
	frame = GetFrame(pageNumber);
	if(frame == -1)
		return CondRetError;
	FrameTable[frame]->ControlBits |= 0x02;
	/*Apenas liga bit R se desligado. Mantém os outros iguais. */
	return CondRetOK;
}

tpCondRet TurnOnBitM(int pageNumber){
	int frame;
	frame = GetFrame(pageNumber);
	if(frame == -1)
		return CondRetError;
	FrameTable[frame]->ControlBits |= 0x01;
	/*Apenas liga bit M se desligado. Mantém os outros iguais. */
	return CondRetOK;
}



/****** FunÃ§oes de acesso a tabela das paginas   ***********/

tpCondRet createPageTable(int numPage){
	int i;
	if(PageTable != NULL)
		return CondRetPageTableExists;

	PageTable = (tpPage**) malloc(sizeof(tpPage*)*numPage);
	if(PageTable == NULL)
		return CondRetError;

	for(i=0; i<numPage;i++){
		PageTable[i] = (tpPage*) malloc(sizeof(tpPage));
		if(PageTable[i] == NULL)
			return CondRetError;
	}
	
	for(i=0; i<numPage;i++)
		PageTable[i]->pageInfo = 0x00000000;
	
	return CondRetOK;
}

int GetFrame(int pageNumber){
	return PageTable[pageNumber]->pageInfo & 0x000003FF;
}

int CheckIfPresent(int pageNumber){
	return ((PageTable[pageNumber]->pageInfo >> 10) & 0x00000001);
}


void TurnOnWrite(int pageNumber){
	PageTable[pageNumber]->pageInfo |= 0x00001000;
	/* Modifica apenas bit W. Os outros ficam inalterados */
}

void TurnOnRead(int pageNumber){
	PageTable[pageNumber]->pageInfo |= 0x00002000;
	/* Modifica apenas bit R. Os outros ficam inalterados */
}

void TurnOnPA(int pageNumber){
	PageTable[pageNumber]->pageInfo |= 0x00000400;
}



/****** Algoritmo NRU ***************************/

tpCondRet NRU(int numPage, int numFrame, int pageNumber, char rw){   
	unsigned char FrameControl = '\0';
	int FrameNumber, i=0, PageToChange=-1, classFlag=0;
	tpCondRet CondRet = CondRetOK	;

	/* Busca página a ser removida */
	do{
		if (i != pageNumber){
			
			if (CheckIfPresent(i)){
	
				FrameNumber = GetFrame(i);
				if(FrameNumber == -1)
					return CondRetError;

				FrameControl = FrameTable[FrameNumber]->ControlBits;
				/* Checa se Pertence as classes 1 ou 2 */
				if ((FrameControl & 0x02) == 0){
					if ((FrameControl & 0x01) == 0){
						/* Se classe  = 1, seleciona para mudar e sai */
						PageToChange = i;
						break;
					} else if( classFlag != 2){
						/* Se classe = 2, coloca como candidato e anuncia que sua classe e 2 */
						classFlag = 2;
						PageToChange = i;
					}
				}
			
				if (classFlag != 2 && classFlag != 3 ){
					if ((FrameControl & 0x01) == 0){	
						classFlag = 3;
						PageToChange = i;
					} else if (classFlag == 0){
						classFlag = 4;
						PageToChange = i;
					}
				}
			}
		}	
		i++;
	} while ( i < numPage);

	/* Checa se encontrou */
	if (PageToChange == -1){
		printf("Erro ao tentar remover pagina da memoria\n");
		return CondRetError;
	}
	/*Remove da memória real e insere nova página corrigindo a tabela de paginas*/
	CondRet = EvictPageFromMemory(PageToChange);
	if(CondRet != CondRetOK)
		return CondRet;
	CondRet = InsertPageToMemory(pageNumber, numFrame, rw);
	if(CondRet != CondRetOK)
		return CondRet;

	/* Retorna OK ou informando que foi feita a escrita em disco */
	if (classFlag == 2 || classFlag == 4)
		return CondRetDirty;
	return CondRetOK;		
}



/****** Algoritmo LRU ***************************/

tpCondRet LRU(int numPage, int numFrame, int pageNumber, char rw){  
	unsigned char FrameControl, BestAge = 0xFF;
	int FrameNumber, i=0, PageToChange=-1, classFlag=0, isDirty=0;
	tpCondRet CondRet;

	/* Procura página a ser removida */
	do{
		if (i != pageNumber){
			/* Checa se presente na memoria */
			if (CheckIfPresent(i)){
				FrameNumber = GetFrame(i);
				FrameControl = FrameTable[FrameNumber]->ControlBits;
				/* Paginas referenciadas apenas são selecionadas se não houverem paginas não referenciadas */
				if ((FrameControl & 0x02) == 0){
					if (LRUCompareAge(FrameTable[FrameNumber]->Age, BestAge)){
						BestAge = FrameTable[FrameNumber]->Age;
						PageToChange = i;
						classFlag = 1;
						if((FrameControl & 0x01) != 0)
							isDirty = 1;
						else
							isDirty = 0; 
					}
				} else if (classFlag == 0){
					if (LRUCompareAge(FrameTable[FrameNumber]->Age, BestAge)){
						BestAge = FrameTable[FrameNumber]->Age;
						PageToChange = i;
						if((FrameControl & 0x01) != 0)
								isDirty = 1;
							else
								isDirty = 0;
					}
				}
			}
		}
		i++;	
	} while ( i < numPage);
	/* Checa se encontrou */
	if (PageToChange == -1){
		printf("Erro ao tentar remover pagina da memoria\n");
		return CondRetError;
	}

	/*Remove da memória real e insere nova página corrigindo a tabela de paginas*/
	CondRet = EvictPageFromMemory(PageToChange);
	if(CondRet != CondRetOK)
		return CondRet;
	CondRet = InsertPageToMemory(pageNumber, numFrame, rw);
	if(CondRet != CondRetOK)
		return CondRet;
	
	/* Retorna OK ou informando que foi feita a escrita em disco */
	if(isDirty == 1)
		return CondRetDirty;
	return CondRetOK;
}

void LRUAge(int numPageFrame){
	int i;
	for(i=0; i<numPageFrame; i++){
		if(FrameTable[i]->TimeofLastAcess != 0){
			FrameTable[i]->Age = FrameTable[i]->Age >> 1; 
			if(FrameTable[i]->ControlBits & 0x02){
				FrameTable[i]->ControlBits &= 0x01;
				FrameTable[i]->Age |= 0x80;
			}
		}
	}		
}

int LRUCompareAge(unsigned char currAge, unsigned char bestAge){
	int i, numcurrAge=0, numbestAge=0;

	for(i=0; i<SIZE_AGE; i++){
		if(currAge & 0x01)
			numcurrAge += power(i);
		if(bestAge & 0x01)
			numbestAge += power(i);
		currAge = currAge >> 1;
		bestAge = bestAge >> 1;
	}
	
	if(numcurrAge > numbestAge)
		return 0;
	return 1;
}



/****** Algoritmo Segunda Chance ****************/

tpCondRet SEG(int numPage, int numFrame, int pageNumber, char rw, int time){ 
	unsigned char FrameControl;
	int FrameNumber, isDirty=0;
	tpSEGList *firstElem;
	tpSEGList *currentElem;
	tpCondRet CondRet;

	if(SEGList == NULL)
		return CondRetMemoryIsEmpty;

	/* Guarda primeiro elemento visto */
	firstElem = SEGList;
	currentElem = firstElem;

	do{
		currentElem = currentElem->ptProximo;
		FrameNumber = GetFrame(currentElem->pageNumber);
		FrameControl = FrameTable[FrameNumber]->ControlBits;
		/* Checa se foi modificada */
		if(FrameControl & 0x01)
			isDirty = 1;
		else
			isDirty = 0;
		/* Checa se é igual ao primeiro elemento. Caso seja checa o bit referência para saber se já foi dada uma volta completa. */
		if (currentElem == firstElem && ((FrameControl & 0x02) == 0))
			break;
		/* Se elemento não é referenciado, deve ser removido */
		if ((FrameControl & 0x02) == 0)
			break;
		SEGMoveToLast(currentElem->pageNumber, time); 			
	} while (1);
	
	/*Remove da memória real e insere nova página corrigindo a tabela de paginas*/
	CondRet = EvictPageFromMemory(currentElem->pageNumber);
	if(CondRet != CondRetOK)
		return CondRet;
	CondRet = InsertPageToMemory(pageNumber, numFrame, rw);
	if(CondRet != CondRetOK)
		return CondRet;
	
	/* Remove e insere paginas na lista de segunda chance */
	SEGRemove(currentElem->pageNumber);
	SEGInsert(pageNumber, time);
	
	/* Retorna OK ou informando que foi feita a escrita em disco */
	if(isDirty == 1)
		return CondRetDirty;
	return CondRetOK;
}

tpCondRet SEGInsert(int pageNumber, int time){
	tpSEGList *Elem;
	tpSEGList *currElem;
	Elem = (tpSEGList *) malloc(sizeof(tpSEGList));
	if(Elem == NULL)
		return CondRetError;
		
	Elem->time	 = time;
	Elem->pageNumber = pageNumber;	
	Elem->ptProximo  = NULL;
	if(SEGList == NULL){
		Elem->ptAnterior = NULL;
		SEGList = Elem;
	} else {
		currElem = SEGList;	
		while(currElem->ptProximo != NULL)
			currElem = currElem->ptProximo;
		currElem->ptProximo = Elem;
		Elem->ptAnterior = currElem;
	}
	
	return CondRetOK;	
}

tpCondRet SEGRemove(int pageNumber){
	tpSEGList *currElem;
	if (SEGList == NULL)
		return CondRetMemoryIsEmpty;
	
	currElem = SEGList;
	do{
		if(currElem->pageNumber == pageNumber)
			break;
		currElem = currElem->ptProximo;
	} while (currElem != NULL);
	
	if(currElem == NULL)
		return CondRetNotInMemory;
	
	if(currElem->ptAnterior == NULL && currElem->ptProximo == NULL)
		SEGList = NULL;
	else if(currElem->ptAnterior == NULL){
		SEGList = currElem->ptProximo;
		currElem->ptProximo->ptAnterior = NULL;
	} else if(currElem->ptProximo == NULL)
		currElem->ptAnterior->ptProximo = NULL;
	else{
		currElem->ptAnterior->ptProximo = currElem->ptProximo;
		currElem->ptProximo->ptAnterior = currElem->ptAnterior;
	}
	free(currElem);
	return CondRetOK;
}

tpCondRet SEGMoveToLast(int pageNumber, int time){
	tpSEGList *currElem = NULL;
	tpSEGList *lastElem = NULL;
	int frame = 0;

	if (SEGList == NULL)
		return CondRetMemoryIsEmpty;
	
	lastElem = SEGList;

	do{
		if(lastElem->pageNumber == pageNumber)
			currElem = lastElem;
		lastElem = lastElem->ptProximo;
	} while(lastElem->ptProximo != NULL);
	
	/* checa se está no ultimo elemento */
	if(lastElem->pageNumber == pageNumber)
		currElem = lastElem;
	
	if(currElem == NULL)
		return CondRetNotInMemory;
	
	if(currElem->ptAnterior == NULL && currElem->ptProximo != NULL){
		SEGList = currElem->ptProximo;
		currElem->ptProximo->ptAnterior = NULL;
	} else if(currElem->ptAnterior != NULL && currElem->ptProximo != NULL){
		currElem->ptAnterior->ptProximo = currElem->ptProximo;
		currElem->ptProximo->ptAnterior = currElem->ptAnterior;
	}
	
	if(currElem != lastElem){
		lastElem->ptProximo = currElem;
		currElem->ptAnterior= lastElem;
		currElem->ptProximo = NULL;
	}
	
	frame = GetFrame(currElem->pageNumber);
	FrameTable[frame]->ControlBits &= 0x01;
	FrameTable[frame]->TimeofLastAcess = time;
	
	return CondRetOK;
}




/**** FunÃ§oes auxiliares ****/

int tonumber(char str[]){
	int i=0, mult=1, num, x=0;

	while( str[i] != '\0')
		i++;
	i--;
	while( i>=0){
		switch (str[i]){
			case '0':
				num = 0;
				break;		
			case '1':
				num = 1;
				break;
			case '2':
				num = 2;
				break;
			case '3':
				num = 3;
				break;
			case '4':
				num = 4;
				break;
			case '5':
				num = 5;
				break;
			case '6':
				num = 6;
				break;
			case '7':
				num = 7;
				break;
			case '8':
				num = 8;
				break;
			case '9':
				num = 9;
				break;
		}		
		x +=  num*mult;
		mult *= 10;
		i--;
	}
	return x; 
}

int power(int exp){
	int x = 1;
	while (exp > 0){
		x *= 2;
		exp--;
	}
	return x;
}

void GenerateReport(char *argv[], int contPageFault, int contDirty){
	printf("----------------------------------------------\n");
	printf("\tArquivo de Entrada:		%s\n", argv[2]);
	if(tonumber(argv[4]) > 16)
		printf("\tTamanho da memoria fisica:	%s KB\n", argv[4]);
	else
		printf("\tTamanho da memoria fisica:	%s MB\n", argv[4]);
	printf("\tTamanho das paginas:		%s KB\n", argv[3]);
	printf("\tAlgoritmo utilizado:		%s\n", argv[1]);
	printf("\tNum de faltas de pagina:	%d\n", contPageFault);
	printf("\tNum de paginas escritas:	%d\n", contDirty);
	printf("----------------------------------------------\n\n");
}



void housekeeping(int numPage, int numPageFrame){
	tpSEGList *next = NULL;
	int i;
	for (i= 0; i<numPageFrame; i++)
		free(FrameTable[i]);
	free(FrameTable);

	for(i=0; i<numPage; i++)
		free(PageTable[i]);
	free(PageTable);

	if(SEGList != NULL){
		while( SEGList != NULL ){
			next = SEGList->ptProximo;	
			free(SEGList);
			SEGList = next;
		}
	}
}


void ShowMemory(int time, char *subst_alg,char debug[], char timeInterval[], int numPageFrame){
	int i, num;

	switch (debug[2]){
		case '0':
			num = 0;
			break;		
		case '1':
			num = 1;
			break;
		case '2':
			num = 2;
			break;
	}		
	printf("Para os bits de controle: 0x -> Se x = 0, ambos desligados. Se x = 1, M ligado. Se x = 2, R ligado. Se x = 3, ambos ligados.\n");
	printf("Caso o algoritmo escolhido seja o LRU, e o nivel de detalhe 2,  sera exibida a idade em hexadecimais.\n\n");
	printf("Intervalo de tempo escolhido: %5d\t Nivel de detalhe: %d\n", tonumber(timeInterval), num);
	printf("Tempo atual: %7d\n\n", time);
	for(i=0; i <numPageFrame; i++){
		printf("Quadro de pagina: %04d", i);
		printf("\n");
		printf("Bits de controle:\t%02x", FrameTable[i]->ControlBits);
		printf("\n");
		if(num >= 1){
			printf("Tempo do ultimo acesso:\t%07d",FrameTable[i]->TimeofLastAcess);
			printf("\n");
		}	
		if(num == 2 && (strcmp(subst_alg, "LRU") == 0)){
			printf("Idade:\t\t\t%02x", FrameTable[i]->Age);
			printf("\n");	
		}
	printf("\n\n");
	}

}
