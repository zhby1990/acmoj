#define WAITING 0
#define COMPLIERING 10
#define RUNNING 9
#define CE 8
#define WA 5
#define TLE 3
#define OLE 7
#define RE 6
#define PE 2
#define AC 1
#define SYSTEM_ERR 11
#define MLE 4
#define RF 12 
#define STATUS_SIG 7
#define STATUS_AC 8
#define host "localhost"
#define user "root"
#define password ""
#define database "judgeonline"
#define MAXPOSIX 8
#define MAXPROBLEM 2000
//#define MAXTXTSIZE 

#include <sys/syscall.h>

int  SYS_J[256]={SYS_rt_sigprocmask, SYS_futex, SYS_read, SYS_open, SYS_close, SYS_execve, SYS_access, SYS_brk, SYS_readlink, SYS_munmap, SYS_close, SYS_uname, SYS_clone, SYS_uname, SYS_mprotect, SYS_rt_sigaction, SYS_getrlimit,   SYS_getgid, SYS_geteuid, SYS_getegid, SYS_set_thread_area, SYS_set_tid_address, SYS_set_robust_list, SYS_exit_group, SYS_arch_prctl ,SYS_mmap,-1};
int SYS_C[256]={SYS_read, SYS_uname, SYS_write, SYS_open, SYS_close, SYS_execve, SYS_access, SYS_brk, SYS_munmap, SYS_mprotect,  SYS_fstat, SYS_set_thread_area, SYS_exit_group,SYS_arch_prctl,SYS_mmap,-1};

typedef struct {
	int solution_id;
	char user_id[30];
	int problem_id,contest_id;
    //double time,memory;
	double stime,ntime;
	double run_t,run_m;
	int language,result,num;
}Submits;

struct probleminfo
{
    double time,memory;
    bool have;
}pinfo[MAXPROBLEM];

inline void push(Submits *submit);
inline void get_queue(Submits *submit);
inline void mysql_connection();
inline void stop();
inline void update_status(int getid,int id,int st);
inline void update_result(int getid,Submits *submit);
inline void update_submit_result(int getid,Submits *submit);
inline void update_user_problem_result(int getid,Submits *submit);
inline void update_contest_result(int getid,Submits *submit);
inline char get_letter(FILE *in);
inline bool is_letter(char a);
inline char get_letter(FILE *in);
inline bool check(FILE *in1,FILE *in2);
inline void create_pthread();
void *thread_work(void *arg);
inline bool judger(int getid,Submits *submit);
inline int get_nowmem(int a);
inline bool compile(int getid,Submits *submit);
inline void init();
inline  void get_pinfo(int getid,int problem_id);
