#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<mysql.h>
#include<unistd.h>
#include<fcntl.h>
#include<limits.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/user.h>
#include<sys/ptrace.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<sys/time.h>
#include<sys/resource.h>
#include<sys/reg.h>
#include"config.h"


MYSQL my_connection[MAXPOSIX+1];
pthread_mutex_t work_mutex;
sem_t bin_sem;
int tid,num;
int csafecall[512],jsafecall[512];
int head,end;
Submits Q[200];

inline void push(Submits submit)
{
    Q[end]=submit;
    end=(end+1)%200;
    num++;
}

inline bool  get_queue(Submits *submit)
{
    if(head!=end)
    {
	submit=&Q[head];
	head=(head+1)%200;
	num--;
	return true;
    }
    return false;
}

inline void mysql_connection()
{
    for(int i=0;i<=MAXPOSIX;i++)
    {
	mysql_init(&my_connection[i]);
	if(mysql_real_connect(&my_connection[i],host,user,password,database,0,NULL,0))
	{
	    printf("connection success\n");
	}
	else
	{
	    printf("Database connection failure:%d,%s\n",mysql_errno(&my_connection[i]),mysql_error(&my_connection[i]));
	    exit(EXIT_FAILURE);
	}
    }
    
}

inline void stop()
{
    for(int i=0;i<=MAXPOSIX;i++)
    {
	mysql_close(&my_connection[i]);
    }
    sem_destroy(&bin_sem);
    exit(EXIT_SUCCESS);
}

inline void update_status(int getid,int id,int st)
{
    char sql[300];
    sprintf(sql,"update solution set result=%d where solution_id='%d';",st,id);
    mysql_query(&my_connection[getid],sql);
    return;
}

inline void update_result(int getid,Submits *submit)
{
    update_submit_result(getid,submit);
    if(submit->result==AC) update_user_problem_result(getid,submit);
    if(submit->contest_id!=0)
    {
	update_contest_result(getid,submit);
    }
    return;
}

inline void update_submit_result(int getid,Submits *submit)
{
    char sql[300];
    sprintf(sql,"update solution set result='%d',time='%d',memory='%d' where solution_id='%d';",submit->result,(int)(submit->run_t),(int)(submit->run_m),submit->solution_id);
    mysql_query(&my_connection[getid],sql);
    return;
}

inline void update_user_problem_result(int getid,Submits *submit)
{
    char sql[300];
    MYSQL_RES *res;
    MYSQL_ROW sqlrow;
    sprintf(sql,"update problem set accepted=accepted+1 where problem_id='%d'",submit->problem_id);
    mysql_query(&my_connection[getid],sql);
    sprintf(sql,"select  problem_id from solution where result='1' and user_id='%s' and problem_id'%d';",submit->user_id,submit->problem_id);
    mysql_query(&my_connection[getid],sql);
    res=mysql_use_result(&my_connection[getid]);
    if(!mysql_num_rows(res))
    {
	sprintf(sql,"update users set solved=solved+1 where user_id='%s';",submit->user_id);
	mysql_query(&my_connection[getid],sql);
    }
    mysql_free_result(res);
}

inline void update_contest_result(int getid,Submits *submit)
{
    char sql[300];
    MYSQL_RES *res;
    MYSQL_ROW row;
    int IS_AC=-1,Wrong=0;
    char pro='A'+submit->num;
    sprintf(sql,"select %c_time,%c_WrongSubmits from attend where contest_id='%d' and user_id='%s';",pro,pro,submit->contest_id,submit->user_id);
    mysql_query(&my_connection[getid],sql);
    res=mysql_store_result(&my_connection[getid]);
    if((row=mysql_fetch_row(res))!=NULL)
    {
	sscanf(row[0],"%d",&IS_AC);
	sscanf(row[1],"%d",&Wrong);
    }
    mysql_free_result(res);
    if(submit->result==AC&&IS_AC==0)
    {
	int ACTIME=(int)(submit->ntime)/60;
	sprintf(sql,"update attend set %c_time='%d',accepts=accepts+1,cost=cost+%d where contest_id='%d' and user_id='%s';",pro,ACTIME,(ACTIME+20*Wrong),submit->contest_id,submit->user_id);
	mysql_query(&my_connection[getid],sql);
	sprintf(sql,"update contest_result set accepted=accepted+1,submit=submit+1 where contest_id='%d' and num='%d';",submit->contest_id,submit->num);
	mysql_query(&my_connection[getid],sql);
    }
    else if(IS_AC==0)
    {
	sprintf(sql,"update attend set %c_WrongSubmits=%c_WrongSubmits+1 where contest_id='%d' and user_id='%s';",pro,pro,submit->contest_id,submit->user_id);
	mysql_query(&my_connection[getid],sql);
	sprintf(sql,"update contest_result set submit=submit+1 where contest_is='%d' and num='%d';",submit->contest_id,submit->num);
	mysql_query(&my_connection[getid],sql);
    }
    return;
}

inline bool is_letter(char a)
{
    return a!='\n'&&a!=' '&&a!='\t'&&a!='\b'&&a!='\r'; 
}

inline char get_letter(FILE *in)
{
    char a;
    while(fscanf(in,"%c",&a)!=EOF)
    {
	if(is_letter(a))
	    return a;
    }
    return char(NULL);
}

inline bool check(FILE *in1,FILE *in2)
{
    char a,b,c;
    bool PEflag=false;
    while(true)
    {
	if(fscanf(in1,"%c",&a)==EOF)
	{
	    if(fscanf(in2,"%c",&b)==EOF)
	    {
		return PEflag?-1:1;
	    }
	    else if(is_letter(b))
	    {
		return 0;
	    }
	    c=get_letter(in2);
	    if(c==-1)return -1;
	    else return 0;
	}
	if(fscanf(in2,"%c",&b)==EOF)
	{
	    if(is_letter(a))
	    {
		return 0;
	    }
	    c=get_letter(in1);
	    if(c==-1)return -1;
	    else return 0;
	}
	if(a!=b)
	{
	    PEflag=true;
	    if(is_letter(a))
	    {
		if(is_letter(b))
		{
		    return 0;
		}
		c=get_letter(in2);
		if(a!=c)return 0;
		continue;
	    }
	    if(is_letter(b))
	    {
		c=get_letter(in1);
		if(b!=c)return 0;
		continue;
	    }
	    a=get_letter(in1);
	    b=get_letter(in2);
	    if(a!=b)return 0;
	}
    }
}

inline void create_pthread()
{
    pthread_t thread[MAXPOSIX];
    for(int i=0;i<MAXPOSIX;i++)
    {
	tid++;
	int res=pthread_create(&(thread[i]),NULL,thread_work,(void *)NULL);
	if(res!=0)
	{
	    printf("Thread creat failed!\n");
	    exit(EXIT_FAILURE);
	}
	sleep(2);
    }
}

void *thread_work(void *arg)
{
    int getid=tid;
    Submits *submit;
    char cmd[300];
    while(true)
    {
	sem_wait(&bin_sem);
	pthread_mutex_lock(&work_mutex);
	if(num==0)
	{
	    exit(EXIT_FAILURE);
	}
	get_queue(submit);
	pthread_mutex_unlock(&work_mutex);
	submit->result=COMPLIERING;
	update_status(getid,COMPLIERING,submit->solution_id);
	if(!pinfo[submit->problem_id].have)
	{
	    get_pinfo(getid,submit->problem_id);
	}
	    
	compile(getid,submit)&&judger(getid,submit);
	update_result(getid,submit);
	if(submit->language==3)
	{
	    sprintf(cmd,"rm -rf ./tmp/%d.java ./tmp/%d ./tmp/%d.out",submit->solution_id,submit->solution_id,submit->solution_id);
	}
	else sprintf(cmd,"rm -rf ./tmp/%d.cpp ./tmp/%d ./tmp/%d.out",submit->solution_id,submit->solution_id,submit->solution_id);
	system(cmd);
    }
}

inline bool judger(int getid,Submits *submit)
{
    pid_t pid;
    fflush(stdin);
    fflush(stdout);
    if((pid=vfork())==0)
    {
	char path[300];
	sprintf(path,"./data/%d/data.in",submit->problem_id);
	freopen(path,"r",stdin);
	sprintf(path,"./tmp/%d.out",submit->solution_id);
	freopen(path,"w",stdout);
	struct rlimit tim,mem;
	tim.rlim_cur=(int)pinfo[submit->problem_id].time/1000;
	mem.rlim_cur=(int)(pinfo[submit->problem_id].memory)*1024;
	if(submit->language==3)
	{
	    tim.rlim_cur=tim.rlim_cur*3;
	    mem.rlim_max=mem.rlim_cur*3;
	}
	tim.rlim_max=tim.rlim_cur;
	mem.rlim_max=mem.rlim_cur;
	setrlimit(RLIMIT_CPU,&tim);
	setrlimit(RLIMIT_DATA,&mem);
	sprintf(path,"./tmp/%d",submit->solution_id);
	ptrace(PTRACE_TRACEME,0,NULL,NULL);
	if(submit->language==3)
	{
	    execlp("java",path,NULL);
	}
        else
	{
	    execl(path,NULL,NULL);
	}
	return false;
    }
    int status,mem,nowmem,file,sig;
    bool flag=true;
    long syscallID;
    struct rusage usage;
    char path[300];
    sprintf(path,"/proc/%d/statm",int(pid));
    file=open(path,O_RDONLY);
    mem=(int)(pinfo[submit->problem_id].memory*1024);
    if(submit->language==3)mem=mem*2;
    submit->result=AC;
    submit->run_m=0;
    submit->run_t=0;
    while(true)
    {
	wait4(pid,&status,0,&usage);
	if(WIFEXITED(status))break;
	if(WIFSIGNALED(status))
	{
	    sig=WTERMSIG(status);
	    switch (sig)
	    {
	    case SIGXCPU : submit->result=TLE;break;
	    case SIGFPE  :
	    case SIGSEGV :submit->result=RE;break;
	    default: submit->result=MLE;
	    }
	    break;
	}
	if(flag)
	{
	    syscallID=ptrace(PTRACE_PEEKUSER,pid,4*ORIG_EAX,NULL);
	    flag=true;
	    if((submit->language==3)?(jsafecall[syscallID]==0):(csafecall[syscallID]==0))
	    {
		submit->result=RF;
		ptrace(PTRACE_KILL,pid,NULL,NULL);
		break;
	    }
	}
	else flag=false;
	nowmem=get_nowmem(file);
	if(nowmem>mem)
	{
	    submit->result=MLE;
	    ptrace(PTRACE_KILL,pid,NULL,NULL);
	    break;
	}
	if(nowmem>submit->run_m)
	{
	    submit->run_m=nowmem;
	}
	ptrace(PTRACE_SYSCALL,pid,NULL,NULL);
    }
    if(submit->result==AC)
    {
	sprintf(path,"./tmp/%d.out",submit->solution_id);
	FILE *a=fopen(path,"r");
	sprintf(path,"./data/%d/data.out",submit->solution_id);
	FILE *b=fopen(path,"r");
	int tmp=check(a,b);
	switch (tmp)
	{
	case 1  :submit->result=AC;break;
	case -1 :submit->result=WA;break;
	case 0  :submit->result=PE;
	}
	submit->run_t+=(usage.ru_utime.tv_sec*1000+usage.ru_utime.tv_usec/1000);
	submit->run_t+=(usage.ru_stime.tv_sec*1000+usage.ru_stime.tv_usec/1000);
	if(submit->run_t > (int)pinfo[submit->problem_id].time)
	{
	    submit->result=TLE;
	}
    }
    if(submit->result!=AC)
    {
	submit->run_m=0;
	submit->run_t=0;
    }
    update_result(getid,submit);
    return true;
}

inline int get_nowmem(int file)
{
    char ss[25];
    memset(ss,0,sizeof(ss));
    pread(file,ss,sizeof(ss),0);
    int i=0;
    while(ss[i]!=' '&&ss[i])i++;
    i++;
    int a=0;
    while(ss[i]!=' '&&ss[i])
    {
	a=a*10+(ss[i]-'0');
	i++;
    }
    return a;
}

inline bool compile(int getid,Submits *submit)
{
    MYSQL_RES *res=NULL;
    MYSQL_ROW row=NULL;
    char sql[300],cmd[300],path[300];
    FILE *file;
    sprintf(sql,"select source from source_code where solution_id='%d';",submit->solution_id);
    if(mysql_query(&my_connection[getid],sql))
    {
	printf("%s\n",mysql_error(&my_connection[getid]));
	return false;
    }
    res=mysql_use_result(&my_connection[getid]);
    row=mysql_fetch_row(res);
    mysql_free_result(res);
    if(submit->language==3)
    {
	sprintf(path,"./tmp/%d.java",submit->solution_id);
	file=fopen(path,"wb");
	fprintf(file,"%s",row[0]);
	fclose(file);
	sprintf(cmd,"dos2unix ./tmp/%d.java",submit->solution_id);
	sprintf(sql,"java ./tmp/%d.java -d ./tmp/%d 2> ./err/%d",submit->solution_id,submit->solution_id,submit->solution_id);
    }
    else
    {
	sprintf(path,"./tmp/%d.cpp",submit->solution_id);
	file=fopen(path,"wd");
	fprintf(file,"%s",row[0]);
	fclose(file);
	sprintf(cmd,"dos2unix ./tmp/%d.cpp",submit->solution_id);
	sprintf(sql,"g++ ./tmp/%d.cpp -o ./tmp/%d -o0 -ansi -w 2> ./err/%d",submit->solution_id,submit->solution_id,submit->solution_id);
    }
    if(system(cmd)==0&&system(sql)==0)	return true;
    submit->result=CE;
    submit->run_t=0;
    submit->run_m=0;
    return false;
}

inline void init()
{
    tid=0;head=0;end=0;num=0;
    mysql_connection();
    create_pthread();
    int rs=pthread_mutex_init(&work_mutex,NULL);
    if(rs!=0)
    {
	printf("Work_mutex initialization failed!\n");
	exit(EXIT_FAILURE);
    }
    rs=sem_init(&bin_sem,0,0);
    if(rs!=0)
    {
	printf("Semaphore initialization failed!\n");
	exit(EXIT_FAILURE);
    }
    for(int i=0;i<MAXPROBLEM;i++)
    {
	pinfo[i].have=false;
    }
    memset(csafecall,0,sizeof(csafecall));
    memset(jsafecall,0,sizeof(jsafecall));
    for(int i=0;SYS_C[i];i++)csafecall[SYS_C[i]]=1;
    for(int i=0;SYS_J[i];i++)jsafecall[SYS_J[i]]=1;
}

inline  void get_pinfo(int getid,int problem_id)
{
    MYSQL_RES *res;
    MYSQL_ROW sqlrow;
    char sql[300];
    sprintf(sql,"select time_limit,memory_limit from problem where problem_id='%d';",problem_id);
    int rs=mysql_query(&my_connection[getid],sql);
    if(rs)
    {
	printf("SELECT error: %s\n",mysql_error(&my_connection[getid]));
	exit(EXIT_FAILURE);
    }
    res=mysql_use_result(&my_connection[getid]);
    if((sqlrow=mysql_fetch_row(res))==NULL)
    {
	printf("Don't have problem %d\n",problem_id);
	exit(EXIT_FAILURE);
    }
    sscanf(sqlrow[0],"%lf",&pinfo[problem_id].time);
    sscanf(sqlrow[1],"%lf",&pinfo[problem_id].memory);
    mysql_free_result(res);
}

int main()
{
    int count=0;
    char sql[300];
    Submits submit;
    MYSQL_RES *res=NULL;
    MYSQL_ROW row=NULL;
    init();
    sprintf(sql,"select solution_id,problem_id,user_id,contest_id,num,stime,ntime,language from solution where result=WAITING order by solution_id limit 10");
    sleep(3);
    while(true)
    {
	
	if(mysql_query(&my_connection[0],sql)!=0)
	{
	    printf("%s\n",mysql_error(&my_connection[0]));
	}
	res=mysql_store_result(&my_connection[0]);
	if(res)
	{
	    printf("mysql store failed!\n");
	    exit(EXIT_FAILURE);
	}
	count++;
	while((row=mysql_fetch_row(res)))
	{
	    sscanf(row[0], "%d", &submit.solution_id);
	    sscanf(row[1], "%d", &submit.problem_id);
	    strcpy(submit.user_id, row[2]);
	    sscanf(row[3], "%d", &submit.contest_id);
	    sscanf(row[4], "%d", &submit.num);
	    sscanf(row[5], "%lf", &submit.stime);
	    sscanf(row[6], "%lf", &submit.ntime);
	    sscanf(row[7], "%d", &submit.language);
	    pthread_mutex_lock(&work_mutex);
	    sem_post(&bin_sem);
	    push(submit);
	    pthread_mutex_unlock(&work_mutex);
	    count=0;
	}
	mysql_free_result(res);
	usleep(300*1000+(num>4?num-4:0)*200*1000);
	if(count>=6000)
	{
	    count=0;
	    for(int i=1;i<=MAXPOSIX;i++)
	    {
		mysql_query(&my_connection[i],";");
	    }
	}
    }
}
