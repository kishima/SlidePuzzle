#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<search.h>
#include<stdarg.h>
#include<time.h>

#define USE_DISTANCE_MAP
#define USE_STACK_CHECK //手戻り監視
#define USE_BIDIRECTION //双方向探索

#define CHECK_TIMEOUT //タイムアウト確認
#define TIMEOUT_SEC 600 //sec

//#define CHECK_ANSWER_MODE //答え確認

//#define LOCAL_DEBUG //デバッグ用


//-----------------------------
#ifdef LOCAL_DEBUG
void dbg(char* text, ...){
	va_list list;
	va_start( list, text );
	vprintf(text, list);
	va_end( list );  
}

#else
#define dbg(fmt, ...)
#endif
//-----------------------------

int LX_MAX,RX_MAX,UX_MAX,DX_MAX;
int TOTAL_PUZZLE;
clock_t g_start_time;
#define MAX_W 6
#define MAX_H 6

#define OPL 0
#define OPR 1
#define OPU 2
#define OPD 3

#define RESULTS_MAX 1
#define RESULTS_MAX_LENGTH 1000
char g_results[RESULTS_MAX][RESULTS_MAX_LENGTH];
char g_results_check_list[5000][RESULTS_MAX_LENGTH];

const char g_trans_int[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',' '};
char g_trans_char2int[256];
void init_trans_map(void){
  int i,c;
  c=0;
  for(i='0';i<='9';i++){
	g_trans_char2int[i]=c;
	c++;
  }
  for(i='A';i<='Z';i++){
	g_trans_char2int[i]=c;
	c++;
  }
  g_trans_char2int['=']=c;
}
const char g_trans_num2op[]={'L','R','U','D'};
const int g_trans_op2num[]=
{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,OPD,-1,-1,-1,-1,-1,-1,-1,OPL,-1,-1,-1,-1,-1,OPR,-1,-1,OPU,-1,-1,-1,-1,-1};
const char g_invert_op2op[]=
{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,'U',-1,-1,-1,-1,-1,-1,-1,'R',-1,-1,-1,-1,-1,'L',-1,-1,'D',-1,-1,-1,-1,-1};

char g_distance_map[256][MAX_W*MAX_H];

struct puzzle_t{
	int width;
	int height;
	int length;
	int init_distance;
	int best_distance;
	int best_depth;
	char data[MAX_W*MAX_H+1];
} g_puzzles[5000];
int g_editmap[5000];

struct stage_t{
	int use;//pool管理用
	int check;//いろいろ
	int hash;//ハッシュ値
	int goal_check;//正解局面との比較済みかどうか
	int current; //0の位置
	char checked[4];//探索済みの枝を示す
	char data[MAX_W*MAX_H+1];

	//HashとQueue
	struct stage_t* prev;
	struct stage_t* next;
	
};

//Stage Pool
#define STAGE_POOL_MAX 1000000
struct stage_t g_stage_pool[STAGE_POOL_MAX];
unsigned int g_stage_pool_current;

//Hash
#define HASH_MAX 200000
struct stage_t* g_hash_table[HASH_MAX];
int g_hash_counter;
int g_hash_hit_count;

//stack
struct stage_t* g_stage_stack[STAGE_POOL_MAX];
unsigned int g_stage_stack_current;

//list
#define LIST_MAX 200000
struct stage_t g_goal_list_stage[LIST_MAX];
unsigned int g_goal_list_current;

//queue
int g_queue_top;
int g_queue_tail;

inline char invert_operation(char op){
	return g_invert_op2op[op];
}

//-----------------
void print_stage(struct stage_t* s,struct puzzle_t* p){
#ifdef LOCAL_DEBUG
	printf("stage:use=%d\n",s->use);
	printf("stage:check=%d\n",s->check);
	printf("stage:data:%s\n",s->data);
#endif
}
void print_matrix(struct stage_t* stage,struct puzzle_t* puzzle){
	int i;
	printf("-------\n");
	for(i=0;i<puzzle->length;i++){
		printf("%c",stage->data[i]);
		if(i%puzzle->width == puzzle->width-1)printf("\n");
	}
}

void print_a(struct stage_t *stage,struct stage_t *stage2,char* text){
	char c[4];
	int i;
	for(i=0;i<4;i++){
		c[i] = stage->checked[i]==0 ? '-' : stage->checked[i];
	}
	dbg("%s:%c %c %c %c: %s\n",text,c[0],c[1],c[2],c[3],stage2->data);
}
//-----------------
//データ読み込み
//-----------------
int load_data(const char* filename,const char* editmap){
	FILE *fp=NULL;
	char line[1024];
	dbg("filename:%s\n",filename);

	if( (fp=fopen(filename,"r")) == NULL){
		fprintf(stderr,"file open error\n");
		return -1;
	}
	dbg("file open ok %p\n",fp);

	fscanf(fp,"%d%d%d%d",&LX_MAX,&RX_MAX,&UX_MAX,&DX_MAX);
	fscanf(fp,"%d",&TOTAL_PUZZLE);
	int i;

	for(i=0;i<TOTAL_PUZZLE;i++){
		fscanf(fp,"%d,%d,%s",&(g_puzzles[i].width),&(g_puzzles[i].height),&(g_puzzles[i].data[0]));
		g_puzzles[i].length = g_puzzles[i].width * g_puzzles[i].height;
	}
	fclose(fp);
#ifndef CHECK_ANSWER_MODE
    if( (fp=fopen(editmap,"r")) == NULL){
		fprintf(stderr,"file open error2\n");
		return -1;
	}
	for(i=0;i<TOTAL_PUZZLE;i++){
	fscanf(fp,"%d",&g_editmap[i]);
    }
    dbg("total:%d\n",TOTAL_PUZZLE);
    fclose(fp);
#else
    if( (fp=fopen(editmap,"r")) == NULL){
        fprintf(stderr,"file open error2\n");
        return -1;
    }
    for(i=0;i<TOTAL_PUZZLE;i++){
        memset(g_results_check_list[i],0,RESULTS_MAX_LENGTH);
    }
    int ret=0;
    int no;
    char text[RESULTS_MAX_LENGTH];
    while(ret!=EOF){
        ret = fscanf(fp,"%d,%s",&no,text);
        if(ret!=EOF){
            strcpy(g_results_check_list[no],text);
        }
    }
    fclose(fp);
#endif
}
//-----------------
//手戻りチェック
//-----------------
int search_stack(struct stage_t* stage){
	int i;
	for(i=g_stage_stack_current-1;i>=0;i--){
		if(0 == strcmp(stage->data, g_stage_stack[i]->data) && stage->checked[0] == g_stage_stack[i]->checked[0]){
			return 1;
		}
	}
	return 0;
}

//-----------------
//Hash
//-----------------
void init_hash(void){
	g_hash_counter = 0;
	g_hash_hit_count = 0;
	int i;
	for(i=0;i<HASH_MAX;i++){
		g_hash_table[i]=NULL;
	}
}

inline unsigned int get_hash_number(char* str,int len){
	int i;
	unsigned int hash;
	long long num=0;
	for(i=0;i<len;i++){
		num = (num<<1) + g_trans_char2int[str[i]];
	}
	hash = (unsigned int)(num % HASH_MAX);
	return hash;
}

void clear_hash(void){
	g_hash_counter = 0;
}

int search_hash_table(struct stage_t* stage,struct puzzle_t* puzzle){
	dbg("search:%s\n",stage->data);
	struct stage_t* temp;

	unsigned int hash = get_hash_number(stage->data,puzzle->length);

	temp = g_hash_table[hash];

	while( temp != NULL ){
		if(0 == strcmp(stage->data, temp->data)){
			return 1;//HIT
		}
		temp = temp->next;
	}
	return 0;
}
void add_hash_table(struct stage_t* stage,struct puzzle_t* puzzle){
  //重複は無い前提
	struct stage_t* tail;
	unsigned int hash = get_hash_number(stage->data,puzzle->length);
	tail = g_hash_table[hash];
	if(tail==NULL){
		g_hash_table[hash]=stage;
		stage->next = NULL;
		g_hash_counter++;
	}else{
		while( tail->next != NULL ){
			tail = tail->next;
		}
		tail->next = stage;
		g_hash_counter++;
	}
}
struct stage_t* find_hash_value(struct stage_t *stage,struct puzzle_t *puzzle){
    
	struct stage_t* temp;    
	unsigned int hash = get_hash_number(stage->data,puzzle->length);
    
	temp = g_hash_table[hash];
    
	while( temp != NULL ){
		if(0 == strcmp(stage->data, temp->data)){
			return temp;
		}
		temp = temp->next;
	}
	return NULL;

}
//-----------------
//正解list
//-----------------
void init_list(void){
	g_goal_list_current=0;
	memset(g_goal_list_stage,0,sizeof(struct stage_t)*LIST_MAX);
}
inline int check_list2(struct stage_t* stage,struct puzzle_t* puzzle){
  int ret;
  ret = search_hash_table(stage,puzzle);
  return ret;
}
inline int check_list(struct stage_t* stage,struct puzzle_t* puzzle){
	return search_hash_table(stage,puzzle);
}

inline int add_list(struct stage_t* stage,struct puzzle_t* puzzle){
	if(g_goal_list_current >= LIST_MAX){
		return -1;
	}
	if( check_list(stage,puzzle) ){//重複を探す
		return -1;
	}
	struct stage_t* new_stage;
	memcpy(&g_goal_list_stage[g_goal_list_current],stage,sizeof(struct stage_t));
	new_stage = &g_goal_list_stage[g_goal_list_current];
	g_goal_list_current++;

	//Hash登録
	add_hash_table(new_stage,puzzle);
	return g_goal_list_current;
}

//-----------------
//queue
//-----------------
void init_queue(void){
	g_queue_top  = 0;
	g_queue_tail = 0;
}
inline int enqueue(struct stage_t* stage,struct puzzle_t* puzzle){
	int current;
	current = add_list(stage,puzzle);//同局面の探索も兼ねる
	if(current<0){
		return -1;
	}
	g_queue_tail = current;
}
inline struct stage_t* dequeue(void){
	struct stage_t* temp;
	if(g_queue_top<g_queue_tail){
		temp = &g_goal_list_stage[g_queue_top];
		g_queue_top++;
	}else{
		temp = NULL;
	}
	return temp;
}

//-----------------
//局面操作
//-----------------
//局面スタック
inline void push_stage(struct stage_t* stage){
	g_stage_stack[g_stage_stack_current] = stage;
	g_stage_stack_current++;
	if(g_stage_stack_current>STAGE_POOL_MAX){
		fprintf(stderr,"stack over flow(%d)\n",__LINE__);
	}
}
inline struct stage_t* pop_stage(void){
	if(g_stage_stack_current>0){
		g_stage_stack_current--;
		struct stage_t* temp = g_stage_stack[g_stage_stack_current];
		g_stage_stack[g_stage_stack_current]=NULL;
		return temp;
	}else{
		return NULL;
	}
}

inline struct stage_t* get_stage_top(void){
	if(g_stage_stack_current>0){
		return  g_stage_stack[g_stage_stack_current-1];
	}else{
		return NULL;
	}
}

//局面用メモリプール
inline struct stage_t* get_stage_mem(void){
	if(g_stage_pool[g_stage_pool_current].use==0){
		g_stage_pool[g_stage_pool_current].use=1;
		unsigned int temp = g_stage_pool_current;
		if(++g_stage_pool_current>=STAGE_POOL_MAX){
			g_stage_pool_current=0;
		}
		return &g_stage_pool[temp];
	}else{
		int i;
		for(i=0;i<STAGE_POOL_MAX;i++){
			if(g_stage_pool[i].use==0){
				g_stage_pool[i].use=1;
				g_stage_pool_current=i;
				if(++g_stage_pool_current>=STAGE_POOL_MAX){
					g_stage_pool_current=0;
				}
				return &g_stage_pool[i];
			}
		}
		fprintf(stderr,"(%d):StageBuffer over flow\n",__LINE__);
		exit(1);
		return NULL;
	}
}
inline void clear_stage(struct stage_t* stage){
	memset(stage,0,sizeof(struct stage_t));
}
void init_stage_pool(void){
	g_stage_stack_current=0;
	g_stage_pool_current=0;

	int i;
	for(i=0;i<STAGE_POOL_MAX;i++){
		g_stage_stack[i]=NULL;
		memset(&(g_stage_pool[i]),0,sizeof(struct stage_t));
	}
}

//局面演算
inline int check_stage_equal(struct stage_t* stage,struct stage_t* goal){
	if(stage->goal_check){
		return stage->goal_check;
	}else{
		return stage->goal_check = strcmp(stage->data,goal->data);
	}
}

inline int calc_distance(char target,int s,struct stage_t* goal,struct puzzle_t* puzzle){
	int x1,x2,y1,y2;
	x1 = s % puzzle->width;
	y1 = s / puzzle->width;
	int i;
	for(i=0;i<puzzle->length;i++){
		if(goal->data[i]==target){
			break;
		}
	}
	x2 = i % puzzle->width;
	y2 = i / puzzle->width;

	return abs(x1-x2) + abs(y1-y2);
	
}

void calc_distance_map(struct stage_t* goal,struct puzzle_t* puzzle){
	int i,j;
	for(i=0;i<puzzle->length;i++){
		for(j=0;j<puzzle->length;j++){
			g_distance_map[g_trans_int[i]][j]=calc_distance(g_trans_int[i],j,goal,puzzle);
		}
	}
}
inline int calc_total_distance(struct stage_t* stage,struct stage_t* goal,struct puzzle_t* puzzle){
	int i;
	int distance=0;
	
	for(i=0;i<puzzle->length;i++){
		if(stage->data[i]!='0' && stage->data[i]!='='){
#ifdef USE_DISTANCE_MAP
			distance += g_distance_map[ stage->data[i] ][i];
#else
			distance += calc_distance(stage->data[i],i,goal,puzzle);
#endif
		}
	}
	
	dbg("distance:%d\n",distance);
	 
	return distance;
}
//データ出力
int output_result(char* result_buf,struct puzzle_t* puzzle){
	int i;
	//print_matrix(g_stage_stack[0],puzzle);
	//printf("result[%02d]:%s\n",i,g_stage_stack[0]->data);
	for(i=1;i<g_stage_stack_current;i++){
		result_buf[i-1] = invert_operation(g_stage_stack[i]->checked[0]);
		//printf("result[%02d]:%s\n",i,g_stage_stack[i]->data);
		//print_matrix(g_stage_stack[i],puzzle);
	}
    return g_stage_stack_current-1;
}
void output_result_fromlink(char* result,struct stage_t* top,struct puzzle_t* puzzle){
    struct stage_t* stage = find_hash_value(top,puzzle);
    int c=0;
    while(stage!=NULL){
        if(stage->prev != NULL){
            result[c]=stage->checked[0];
        }
        stage = stage->prev;
        c++;
    }
    return;
}
//-----------------
//探索
//-----------------

//探索済みかどうか？
inline int set_checked(struct stage_t* stage,char op){
	int i;
	for(i=0;i<4;i++){
		if(stage->checked[i]==op){
			return 1;
		}else if(stage->checked[i]==0){
			stage->checked[i]=op;
			return 0;
		}
	}
	fprintf(stderr,"(%d):is_checked error\n",__LINE__);
	exit(1);
	return 0;
}

//１手後の局面生成にトライ

inline struct stage_t* make_child_stage(struct stage_t* stage,char op,struct puzzle_t* puzzle,struct stage_t*(*get_mem)(void)){
	int width = puzzle->width;
	int height = puzzle->height;
	int current;

	current = stage->current;
	
	switch(op){//check
	case 'L':
		if(current%width==0){//左が壁
			return NULL;
		}
		if(stage->data[current-1]=='='){//左が壁
			return NULL;
		}
		break;
	case 'R':
		if(current%width==width-1){//右が壁
			return NULL;
		}
		if(stage->data[current+1]=='='){//右が壁
			return NULL;
		}
		break;
	case 'U':
		if(current<width){//上が壁
			return NULL;
		}
		if(stage->data[current-width]=='='){//上が壁
			return NULL;
		}
		break;
	case 'D':
		if(current+width>=puzzle->length){//下が壁
			return NULL;
		}
		if(stage->data[current+width]=='='){//下が壁
			return NULL;
		}
		break;
	default:
		fprintf(stderr,"error\n");
		exit(1);
		break;
	}

	struct stage_t* child = get_mem();

	memcpy(child->data,stage->data,puzzle->length);

	char temp;
	switch(op){//operation
	case 'L':
		child->data[current]=stage->data[current-1];
		child->data[current-1]='0';
		child->current=current-1;
		child->checked[0]=invert_operation('L');
		break;
	case 'R':
		child->data[current]=stage->data[current+1];
		child->data[current+1]='0';
		child->current=current+1;
		child->checked[0]=invert_operation('R');
		break;
	case 'U':
		child->data[current]=stage->data[current-width];
		child->data[current-width]='0';
		child->current=current-width;;
		child->checked[0]=invert_operation('U');
		break;
	case 'D':
		child->data[current]=stage->data[current+width];
		child->data[current+width]='0';
		child->current=current+width;
		child->checked[0]=invert_operation('D');
		break;
	default:
		break;
	}
	child->prev = stage;
	
	return child;
}
inline struct stage_t* make_child_stage_depthfirst(struct stage_t* stage,char op,struct puzzle_t* puzzle){
	struct stage_t* child = make_child_stage(stage,op,puzzle,get_stage_mem);

	if(child!=NULL){
	
#ifdef USE_STACK_CHECK
		if( search_stack(child) ){
			clear_stage(child);
			return NULL;
		}
#endif
	}
	
	return child;
}
inline struct stage_t* make_child_stage_widthfirst(struct stage_t* stage,char op,struct puzzle_t* puzzle){
	struct stage_t* child = make_child_stage(stage,op,puzzle,get_stage_mem);
	if(child!=NULL){
		child->check = stage->check + 1;
	}	
	return child;
}

inline struct stage_t* find_child_stage(struct stage_t* stage,struct puzzle_t* puzzle){
	int i;
	print_a(stage,stage,"checked1:");
	for(i=0;i<4;i++){
		dbg("check method:%c\n",g_trans_num2op[i]);
		if(0==set_checked(stage,g_trans_num2op[i])){
			struct stage_t* child = make_child_stage_depthfirst(stage,g_trans_num2op[i],puzzle);
			if(child!=NULL){//操作可能
				print_a(stage,stage,"checked2:");
				return child;
			}
		}
	}
	return NULL;
}

//反復深化探索
int find_path_depth_first(struct stage_t* start,struct stage_t* goal,int target_depth,struct puzzle_t* puzzle){
	start->check = 1;
	push_stage(start);
	struct stage_t* stage;
	struct stage_t* child;
	int result_count=0;
	int depth=0;
	int j;
    unsigned int timeout_check=0;

	while( (stage = get_stage_top()) != NULL){
		dbg("-- depth %d --- : %s\n",depth,stage->data);
#ifdef CHECK_TIMEOUT
        if(timeout_check>=1000000){
            if( (clock()-g_start_time)/CLOCKS_PER_SEC > TIMEOUT_SEC ){
                break;
            }
            timeout_check=0;
        }
        timeout_check++;
#endif
		if(check_stage_equal(stage,goal) == 0){//回答
			output_result(g_results[result_count],puzzle);
			result_count++;
			break;
#ifdef USE_BIDIRECTION
		}else if(check_list2(stage,puzzle)){//逆方向からの回答にぶつかった
            int c;
			dbg("\n*** goal list hit ***\n");
            c = output_result(g_results[result_count],puzzle);
            dbg("output_result_fromlink=%d\n",c);
            output_result_fromlink(&g_results[result_count][c],stage,puzzle);
			dbg("(%02d:%02d)best:%d/%d hash_hit=%d result=%s\n",depth,target_depth,puzzle->best_distance, puzzle->init_distance,g_hash_hit_count,g_results[result_count]);
			result_count++;
			break;
		}else{
#else
		}else{
#endif
			if( depth+1 > target_depth){//反復深度法
				dbg("-- plune %d\n",depth);
				depth--;
				print_a(stage,stage,"checked3");
				clear_stage(pop_stage());
				continue;
			}
			child = find_child_stage(stage,puzzle);

			if(child==NULL){
				depth--;
				print_a(stage,stage,"checked");
				clear_stage(pop_stage());
			}else{
				int distance = calc_total_distance(child,goal,puzzle);
				
				if(distance < puzzle->best_distance){
					puzzle->best_distance = distance;
					puzzle->best_depth = depth;
				}
				 
				
				if( depth + distance > target_depth ){//最低距離により枝狩り
					dbg("plune MD\n");
					clear_stage(child);
				}else{//一段深く潜る
					print_a(stage,child,"down");
					child->check=1;
					depth++;
					push_stage(child);
				}
			}
		}
	}
	return result_count;
}

//正解の局面を作成
void make_goal_stage(struct stage_t* goal,struct puzzle_t* puzzle){
	int i;
	for(i=0;i<puzzle->length;i++){
		if(puzzle->data[i]!='='){
			goal->data[i] = g_trans_int[i+1];
		}else{
			goal->data[i]='=';
		}
	}
	goal->data[puzzle->length - 1]='0';
}

//幅優先探索
int find_path_width_first(struct stage_t* start,struct stage_t* goal,struct puzzle_t* puzzle){
	struct stage_t  body;
	struct stage_t* stage;
	struct stage_t* child;
	int result=1;
	int i;
    
    start->checked[0]=0;
	
	enqueue(start,puzzle);
	
	while( (stage = dequeue()) != NULL){
		dbg("----- : %s\n",stage->data);
		if(0 == check_stage_equal(stage,goal)){
			result=0;
			break;
		}
		for(i=0;i<4;i++){
			child = make_child_stage_widthfirst(stage,g_trans_num2op[i],puzzle);
			if(child==NULL) continue;
			if(enqueue(child,puzzle)<0){//たまりきった|同じ局面
				continue;
			}
			clear_stage(child);
		}
	}
	return result;
}

//解法
int set_current(struct stage_t* stage,struct puzzle_t* puzzle){
	int i;
	for(i=0;i<puzzle->length;i++){
		if(stage->data[i] == '0'){
			stage->current = i;
			return i;
			break;
		}
	}
}
int solve_puzzle(int no){
	struct puzzle_t* puzzle = &g_puzzles[no];
	fprintf(stderr,"[%d]%d x %d puzzle\n",no,puzzle->width,puzzle->height);
	clock_t start_time,end_time;
	start_time = clock();
	g_start_time = start_time;
	
	//Init
	int i,j;
	for(i=0;i<RESULTS_MAX;i++){
		for(j=0;j<RESULTS_MAX_LENGTH;j++){
			g_results[i][j]=0;
		}
	}
	struct stage_t  startbody,goalbody;
	struct stage_t* start = &startbody;
	struct stage_t* goal  = &goalbody;

	init_stage_pool();
	
	//goal
    clear_stage(goal);
	make_goal_stage(goal,puzzle);
	set_current(goal,puzzle);
	  
	int depth = 0;
	int result_count=0;
	int even_odd = 0;

	//start
    clear_stage(start);
	memcpy( start->data, puzzle->data, puzzle->length);
	i = set_current(start,puzzle);
	even_odd = calc_distance('0',i,goal,puzzle) % 2 == 0 ? 0 : 1 ;//偶数手？

#ifdef USE_BIDIRECTION
	dbg("BIDIRECTION\n");
	init_list();
	init_queue();
	init_hash();
	while(result_count==0){
		
		result_count = find_path_width_first(goal,start,puzzle);
		
	}
	int count=0;
	for(i=0;i<HASH_MAX;i++){
		if(g_hash_table[i]!=NULL)count++;
	}
	dbg("hash %d/%d/%d list=%d\n",count,g_hash_counter,HASH_MAX,g_goal_list_current);
	init_stage_pool();
#endif
	
	calc_distance_map(goal,puzzle);

	puzzle->init_distance = calc_total_distance(start,goal,puzzle);
	puzzle->best_distance = puzzle->init_distance;
	
	clear_stage(start);

	result_count=0;
	
	if(puzzle->init_distance > 0){
		if(even_odd==0){
			dbg("even\n");
			depth = puzzle->init_distance % 2 ? puzzle->init_distance -1 : puzzle->init_distance;
		}else{
			dbg("odd\n");
			depth = puzzle->init_distance % 2 ? puzzle->init_distance : puzzle->init_distance -1;
		}
	}else{
		depth = 0;
	}
	
	while(result_count==0 && depth<RESULTS_MAX_LENGTH){
		dbg("\n*******************\n");
		fprintf(stderr,"%d/",depth);
#ifdef CHECK_TIMEOUT
		if((int)((clock()-start_time)/CLOCKS_PER_SEC) >TIMEOUT_SEC ){
			fprintf(stderr,"**skip**\n");
			break;
		}
#endif
		
		//start
        clear_stage(start);
		memcpy( start->data, puzzle->data,puzzle->length);
		set_current(start,puzzle);
		
		result_count = find_path_depth_first(start,goal,depth,puzzle);
		
		//解の深さを検討
		int temp_depth = depth+2;
		if( puzzle->best_distance > 0 ){
#ifdef USE_HEURISTIC02
			depth += (puzzle->best_distance-2);//ヒューリスティック
#else
			depth = puzzle->best_depth + puzzle->best_distance; //<正解
#endif
			if(depth<temp_depth){
				depth = temp_depth;
			}else{
				if(even_odd==0){ 
					depth = depth % 2 ? depth -1 : depth;
				}else{
					depth = depth % 2 ? depth : depth -1;
				}
			}
		}
		 
	}
	end_time = clock();
	fprintf(stderr,"\n");
	printf("result[%5d]:Len=%d:Time=%d:%s\n",no,strlen(g_results[0]),(int)((end_time-start_time)/CLOCKS_PER_SEC),g_results[0]);
	fflush(stdout);

}
int check_result(int no,int op[]){
    char* result = g_results_check_list[no];
    int c=0;
    char operation;
    
    struct puzzle_t* puzzle = &g_puzzles[no];
    struct stage_t startbody,goalbody;
    struct stage_t* start = &startbody;
    struct stage_t* goal = &goalbody;
    struct stage_t* next;
    struct stage_t* current;
    //
    clear_stage(start);
    memcpy( start->data, puzzle->data, puzzle->length);
    set_current(start,puzzle);
    //
    clear_stage(goal);
    make_goal_stage(goal,puzzle);
    set_current(goal,puzzle);

    init_stage_pool();
    
    current=start;
    for(c=0;c<strlen(result);c++){
        operation=result[c];
        
        op[g_trans_op2num[operation]]++;
         
        //printf("Operation=%c\n",result[c]);
        next = make_child_stage(current,operation,puzzle,get_stage_mem);
        if(next!=NULL){
            clear_stage(current);
            current=next;
            
        }else{
            fprintf(stderr,"bad operation\n");
            return 1;
        }
             
        //print_matrix(current,puzzle);
    }
    //printf("==== check\n");
    //print_matrix(current,puzzle);
    //print_matrix(goal,puzzle);
    if(0==strcmp(current->data,goal->data)){
        return 0;
    }else{
        return 1;
    }
     
}

//----------
//MAIN
//----------
int main(int argc,char* argv[]){
    init_trans_map();
	load_data(argv[1],argv[2]);
#ifndef CHECK_ANSWER_MODE
	int i;
	int L;
	int start;
	int end;
	if(argc>2){
		start=atoi(argv[3]);
		end=atoi(argv[4]);
	}else{
		start=0;
		end=TOTAL_PUZZLE;
	}
	for(i=start;i<end;i++){
		L=g_puzzles[i].length;
		if(g_editmap[i]==0){
			solve_puzzle(i);
		}
	}
#else
    int ret;
    int i;
    int op[4];
    memset(op,0,4*sizeof(int));
    for(i=0;i<TOTAL_PUZZLE;i++){
        if(g_results_check_list[i][0]!=0){
            printf("check[%d]>",i);
            ret = check_result(i,op);
        
            if(ret==0){
                printf("NO:%d OK!\n",i);
            }else{
                printf("NO:%d *** NG ***\n",i);
            }
        }
    }
    for(i=0;i<4;i++){
        printf("op[%c]=%d\n",g_trans_num2op[i],op[i]);
    }
    
#endif
}
