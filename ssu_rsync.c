#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <utime.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define SEC_TO_MICRO 1000000

void check_rsync(char* src, char* s_path, char* d_path, int argc, char* argv[]);
void *do_rsync(void *arg);
void ssu_runtime();
static void ssu_signal_handler(int signo);

void (*ssu_func)(int);

struct timeval begin_t, end_t;
char dst_path[BUFFER_SIZE];
char src_path[BUFFER_SIZE];
char b_path[BUFFER_SIZE];
int d_nitems;

struct data{ //src 파일 정보 전달을 위한 구조체
	char filename[30];
	int filesize;
	long mtime;
};

struct logdata{ //로그 작성을 위한 구조체
	char filename[30];
	int filesize;
};

int main(int argc, char *argv[]){
	char src[BUFFER_SIZE];
	char dst[BUFFER_SIZE];
	char s_path[BUFFER_SIZE];
	char d_path[BUFFER_SIZE];
	struct stat statbuf;

	gettimeofday(&begin_t, NULL);

	if(argc < 3){
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}
	
	strcpy(src, argv[1]);
	strcpy(dst, argv[2]);
	
	if(realpath(src, s_path) == NULL || realpath(dst, d_path) == NULL){ //입력받은 경로 찾을 수 없을 때
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	strcpy(dst_path, d_path); //dst 디렉토리 절대 경로 전역변수
	strcpy(src_path, s_path); //src 절대 경로 전역변수

	if((access(s_path, R_OK) < 0) || (access(s_path, W_OK) < 0)){ //src 접근 권한 없을 때
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	if(stat(dst, &statbuf) < 0){
		fprintf(stderr, "stat error for dst\n");
		exit(1);
	}
		
	if(!S_ISDIR(statbuf.st_mode)){ //dst 디렉토리 파일이 아닐 때
		fprintf(stderr, "dst is not dir\n");
		exit(1);
	}

	if((access(d_path, R_OK) < 0) || (access(d_path, W_OK) < 0)){ //dst 접근 권한 없을 때
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	check_rsync(src, s_path, d_path, argc, argv);
}

void check_rsync(char* src, char* s_path, char* d_path, int argc, char*argv[]){ //동기화 여부 검사
	FILE *fp, *fp2, *fp3;
	struct dirent **items, **s_items, **d_items;
	struct stat statbuf1;
	int cnt, nitems, s_nitems, size, status, check=0, num=0;
	char buffer[BUFFER_SIZE], path[BUFFER_SIZE], tmp[BUFFER_SIZE];
	char *ptr;
	char l_path[BUFFER_SIZE];
	struct data s_fileinfo[100];
	struct data d_fileinfo[100];
	struct logdata logdata[100];
	time_t now_time;
	struct tm *now_date;
	char logfile[BUFFER_SIZE];

	ssu_func = signal(SIGINT, ssu_signal_handler);
	
	getcwd(path, BUFFER_SIZE);
	strcpy(b_path, path); 

	if(stat(src, &statbuf1) < 0){
		fprintf(stderr, "stat error\n");
		exit(1);
	}

	chdir(d_path); 
	nitems = scandir(d_path, &items, NULL, alphasort); //dst디렉토리 내부 파일 개수

	chdir(path);
	pthread_t tid[nitems];

	if(!S_ISDIR(statbuf1.st_mode)){ //src가 파일일 때
		cnt = 0;
		//src
		if(src[0] == '/' || src[0] == '.'){ //절대경로 or 상대경로일 경우 파일 이름만 자름
			memset(l_path, 0, BUFFER_SIZE);
			ptr = strrchr(src, '/');
			ptr++;
			strcpy(l_path,ptr);
		}
		else
			strcpy(l_path, src);
		
		s_fileinfo[cnt].filesize = statbuf1.st_size; 
		s_fileinfo[cnt].mtime = statbuf1.st_mtime;
		strcpy(s_fileinfo[cnt].filename,l_path);
		//파일 정보 구조체에 저장
		
		for(int i = 0 ; i < nitems ; i++){ //dst 디렉토리의 파일 개수만큼 반복
			chdir(d_path);
			struct stat statbuf2;
			if((!strcmp(items[i]->d_name,".")) || (!strcmp(items[i]->d_name,".."))){
				continue;
			}
			stat(items[i]->d_name, &statbuf2); //각 파일마다 stat

			d_fileinfo[i].filesize = statbuf2.st_size;
			d_fileinfo[i].mtime = statbuf2.st_mtime;
			strcpy(d_fileinfo[i].filename,items[i]->d_name);
			//dst 디렉토리의 파일들을 각각 구조체에 저장

			if(!strcmp(s_fileinfo[cnt].filename,d_fileinfo[i].filename)){ //파일 이름 같은 게 있을 때
				if((s_fileinfo[cnt].filesize == d_fileinfo[i].filesize) && (s_fileinfo[cnt].mtime == d_fileinfo[i].mtime)){ //동일한 파일이 이미 있을 때 => 동기화 x
					signal(SIGINT, SIG_IGN);
					break;
				}
				else {//이름만 같을 때 => src파일로 대체
					if(pthread_create(&tid[cnt], NULL, do_rsync, (void*)&s_fileinfo[cnt]) != 0){ //동기화 실행
						fprintf(stderr, "pthread_create error\n");
						exit(1);
					}
					//로그 구조체에 저장
					strcpy(logdata[num].filename, s_fileinfo[cnt].filename);
					logdata[num].filesize = s_fileinfo[cnt].filesize;
					num++;
					pthread_join(tid[cnt], NULL); //쓰레드 종료까지 대기
				}
			}
			else{ 
				check++;
			}
		}
		if(check == nitems - 2){ //디렉토리 다 돌아도 같은 파일 없을 때 => src 파일 복사
			if(pthread_create(&tid[cnt], NULL, do_rsync, (void*)&s_fileinfo[cnt]) != 0){ //동기화 실행
				fprintf(stderr, "pthread_create error\n");
				exit(1);
			}
				//로그 구조체에 저장
				strcpy(logdata[num].filename, s_fileinfo[cnt].filename);
				logdata[num].filesize = s_fileinfo[cnt].filesize;
				num++;
				
			pthread_join(tid[cnt], NULL); //쓰레드 종료까지 대기
		}
	}
	else if(S_ISDIR(statbuf1.st_mode)){ //src가 디렉토리일 때
		s_nitems = scandir(s_path, &s_items, NULL, alphasort); //src 디렉토리 파일 개수
		d_nitems = scandir(dst_path, &d_items, NULL, alphasort); //dst 디렉토리 파일 개수 (전역변수)
		for(int i = 0 ; i < s_nitems ; i++){ //src디렉토리의 파일 개수만큼 반복 
			chdir(s_path);
			struct stat statbuf3;
			if((!strcmp(s_items[i]->d_name,"."))||(!strcmp(s_items[i]->d_name,"..")))
				continue;
			stat(s_items[i]->d_name,&statbuf3);
			
			s_fileinfo[i].filesize = statbuf3.st_size;
			s_fileinfo[i].mtime = statbuf3.st_mtime;
			strcpy(s_fileinfo[i].filename, s_items[i]->d_name);
			//파일 정보 구조체에 저장
			memset(l_path, 0, BUFFER_SIZE);
			strcpy(l_path, s_fileinfo[i].filename); //파일명
			check = 0;

			for(int j = 0 ; j < d_nitems ; j++){ //dst 디렉토리 파일 개수만큼 반복
				chdir(d_path);
				struct stat statbuf4;
				if((!strcmp(items[j]->d_name,".")) || (!strcmp(items[j]->d_name,"..")))
					continue;
				stat(items[j]->d_name, &statbuf4);

				d_fileinfo[j].filesize = statbuf4.st_size;
				d_fileinfo[j].mtime = statbuf4.st_mtime;
				strcpy(d_fileinfo[j].filename, items[j]->d_name);
				//dst 디렉토리의 파일들을 각각 구조체에 저장
				if(!strcmp(s_fileinfo[i].filename,d_fileinfo[j].filename)){ //파일 이름 같은 게 있을 때
					if((s_fileinfo[i].filesize == d_fileinfo[j].filesize) && (s_fileinfo[i].mtime == d_fileinfo[j].mtime) && d_nitems == 2){ //dst 디렉토리에 파일 존재하지 않을 때 => 첫 동기화 
						break;
					}
					else if((s_fileinfo[i].filesize == d_fileinfo[j].filesize) && (s_fileinfo[i].mtime == d_fileinfo[j].mtime) && d_nitems != 2){ //파일 이름 같은 게 있을 때
						if(pthread_create(&tid[i], NULL, do_rsync, (void*)&s_fileinfo[i]) != 0){ //동기화 실행
							fprintf(stderr, "pthread_create error\n");
							exit(1);
						}
						pthread_join(tid[i], NULL);
						break;
					}
					else {//이름만 같을 때 => src파일로 대체
						if(pthread_create(&tid[i], NULL, do_rsync, (void*)&s_fileinfo[i]) != 0){ //동기화 실행
							fprintf(stderr, "pthread_create error\n");
							exit(1);
						}
							//로그 구조체에 저장
							strcpy(logdata[num].filename, l_path);
							logdata[num].filesize = s_fileinfo[i].filesize;
							num++;
					
						pthread_join(tid[i], NULL); //쓰레드 종료까지 대기
					}
				}
				else{ 
					check++;
				}
				
			}
			chdir(s_path);
			if(check == nitems - 2){ //디렉토리 다 돌아도 같은 파일 없을 때 => src 파일 복사
				if(pthread_create(&tid[i], NULL, do_rsync, (void*)&s_fileinfo[i]) != 0){ //동기화 실행
					fprintf(stderr, "pthread_create error\n");
					exit(1);
				}
					//로그 구조체에 저장
					strcpy(logdata[num].filename, l_path);
					logdata[num].filesize = s_fileinfo[i].filesize;
					num++;
			
				pthread_join(tid[i], NULL); //쓰레드 종료까지 대기
			}
		
		} //for
	} //else if

	//로그 작성
	chdir(path);
	if((fp3 = fopen("ssu_rsync_log", "a+")) == NULL){
		fprintf(stderr, "fopen error for ssu_rsync_log\n");
		exit(1);
	}

	time(&now_time);
	now_date = localtime(&now_time);
	sprintf(tmp, "[%s", asctime(now_date));
	tmp[strlen(tmp)-1] = '\0';
	strcat(tmp, "]");

	for(int i = 0 ; i < argc ; i++){
		strcat(tmp, " ");
		strcat(tmp, argv[i]);
	}

	fwrite(tmp, strlen(tmp), 1, fp3);
	
	for(int i = 0 ; i <num ; i++){
		memset(logfile, 0, BUFFER_SIZE);
		if(i == num-1){
			sprintf(logfile, "\n\t%s %dbytes\n", logdata[i].filename, logdata[i].filesize);
			fwrite(logfile, strlen(logfile), 1, fp3);
			break;
		}
		sprintf(logfile, "\n\t%s %dbytes", logdata[i].filename, logdata[i].filesize);
		fwrite(logfile, strlen(logfile), 1, fp3);
	}

	if(num == 0){
		strcpy(logfile, "\n");
		fwrite(logfile, strlen(logfile), 1, fp3);
	}

	gettimeofday(&end_t, NULL);
	ssu_runtime();
	exit(0);
}

void *do_rsync(void *arg){ //동기화 실행
	FILE *fp, *s_fp;
	FILE *t_fp;
	struct utimbuf timebuf1;
	struct utimbuf timebuf2;
	struct data *data;
	struct stat statbuf;
	struct stat statbuf2;
	char filename[30], tmpfilename[30], buf[BUFFER_SIZE], buffer[BUFFER_SIZE], check[BUFFER_SIZE];
	char *ptr;
	int filesize, size, s_size, fsize;
	long mtime;
	
	chdir(dst_path); //dst 디렉토리로 이동
	data = (struct data*)arg; //인자로 받은 s_fileinfo
	
	memset(filename, 0, sizeof(filename));
	memset(buf, 0, sizeof(buf));
	memset(buffer, 0, sizeof(buffer));

	strcpy(filename, data->filename);
	filesize = data->filesize;
	mtime = data->mtime;

	timebuf1.modtime = mtime; //원본 파일의 수정시간

	sprintf(tmpfilename, "%s.tmp", filename); //SIG_INT시 처리를 위한 복사본 파일

	if(access(filename, F_OK) == 0){ //동일한 이름의 파일이 이미 존재한다면 tmp파일 만들어줌
		if(stat(filename, &statbuf) < 0){ 
			fprintf(stderr, "stat error2\n");
			exit(1);
		}

		timebuf2.actime = statbuf.st_atime;
		timebuf2.modtime = statbuf.st_mtime;

		if((fp = fopen(filename, "r+")) == NULL){
			fprintf(stderr, "fopen error\n");
			exit(1);
		}

		if((t_fp = fopen(tmpfilename, "w+")) == NULL){ //tmp파일 생성
			fprintf(stderr, "fopen error for tmpfilename\n");
			exit(1);
		}

		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
	
		while(size > 0){
			memset(buffer, 0, BUFFER_SIZE);
			if(size > BUFFER_SIZE){ //내용이 1024보다 클 때
				fsize = BUFFER_SIZE;
			}
			else{
				fsize = size;
			}
			fread(buffer, fsize, 1, fp);
			fwrite(buffer, fsize, 1, t_fp); //파일 내용을 tmp파일에 복사
			size -= fsize;
		}
	
		fclose(t_fp);
		fclose(fp);	
		chdir(dst_path);
		
		if(utime(tmpfilename, &timebuf2) < 0){ //tmp 파일 수정 시간 일치시켜줌
			fprintf(stderr, "utime error1\n");
			exit(1);
		}
	}

	if((fp = fopen(filename, "w+")) == NULL){ //동기화 파일 오픈
		fprintf(stderr, "fopen error w+\n");
		exit(1);
	}

	ptr = strrchr(src_path, '/');
	ptr++;
	strcpy(check, ptr); //check는 src 파일명 or 디렉토리명
	
	chdir(b_path); 

	if(stat(check, &statbuf2) < 0){
		fprintf(stderr, "stat error1\n");
		exit(1);
	}

	if(S_ISDIR(statbuf2.st_mode)) //디렉토리면 src 해당 디렉토리로 이동
		chdir(src_path);
	else
		chdir(b_path);
	
	if((s_fp = fopen(filename, "r+")) == NULL){
		fprintf(stderr, "fopen error\n");
		exit(1);
	}

	fseek(s_fp, 0, SEEK_END);
	s_size = ftell(s_fp);
	fseek(s_fp, 0, SEEK_SET);

	while(s_size > 0){
		memset(buf, 0, BUFFER_SIZE);
		if(s_size > BUFFER_SIZE){ //파일 크기가 1024보다 클 때
			fsize = BUFFER_SIZE;
		}
		else{
			fsize = s_size;
		}
	
		if(fread(buf, fsize, 1, s_fp) != 1){ //src 파일 내용 동기화 파일에 복사
			fprintf(stderr, "fread error 1\n");
			exit(1);
		}
		fwrite(buf, fsize, 1, fp);
		s_size -= fsize;
	}

	fclose(s_fp);
	fclose(fp);

	chdir(dst_path);

	if(utime(filename, &timebuf1) < 0){ //동기화 파일 수정 시간 일치시켜줌
		fprintf(stderr, "utime error\n");
		exit(1);
	}

	return NULL;
}

void ssu_runtime(){ //프로그램 실행 시간 측정 함수
	end_t.tv_sec -= begin_t.tv_sec;

	if(end_t.tv_usec < begin_t.tv_usec){
		end_t.tv_sec--;
		end_t.tv_usec += SEC_TO_MICRO;
	}

	end_t.tv_usec -= begin_t.tv_usec;

	printf("Runtime %ld:%06ld(sec:usec)\n", end_t.tv_sec, end_t.tv_usec);
}

static void ssu_signal_handler(int signo){
	FILE *fp1, *fp2;
	char check[BUFFER_SIZE];
	char tmpfilename[BUFFER_SIZE], tmpfilebuf[BUFFER_SIZE];
	char filebuf[BUFFER_SIZE];
	char *ptr;
	struct dirent **items;
	struct stat statbuf;
	struct utimbuf timebuf1, timebuf2;
	int size1, size2, nitems;
	long mtime;

	chdir(dst_path);

	if(signo == SIGINT){ //SIGINT 발생시
		ptr = strrchr(src_path, '/');
		ptr++;
		strcpy(check, ptr); //check는 src 파일명 or 디렉토리명
		chdir(b_path);
		if(stat(check, &statbuf) < 0){
			fprintf(stderr, "stat error\n");
			exit(1);
		}
		if(S_ISDIR(statbuf.st_mode)){ //src가 디렉토리일 때
			chdir(src_path); //src 디렉토리로 이동
			nitems = scandir(".", &items, NULL, alphasort);

			for(int i = 0 ; i < nitems ; i++){ //src 디렉토리 파일 개수만큼 반복
				chdir(dst_path); //dst 디렉토리로 이동

				if(!strcmp(items[i]->d_name,".") || !strcmp(items[i]->d_name,".."))
					continue;

				struct stat fstat;
				if(stat(items[i]->d_name, &fstat) < 0){
					fprintf(stderr, "stat error for %s \n", items[i]->d_name);
					exit(1);
				}

				memset(filebuf, 0, BUFFER_SIZE);
				memset(tmpfilebuf, 0, BUFFER_SIZE);
				memset(tmpfilename, 0, BUFFER_SIZE);

				sprintf(tmpfilename, "%s.tmp", items[i]->d_name); //내부 파일마다

				if(access(tmpfilename, F_OK) < 0){ //첫 동기화 파일이면 바로 파일 지움
					remove(items[i]->d_name);
					continue;
				}
				else{ //파일이름 같고 파일 내용 바뀌었을 때 동기화
					remove(items[i]->d_name); //동기화 파일 지우고

					if(rename(tmpfilename, items[i]->d_name) < 0){ //tmp파일을 동기화 파일 이름으로 변경
						fprintf(stderr, "rename error\n");
						exit(1);
					}
				}

			}
		}
		else{ //src가 파일일 때
			chdir(dst_path);
			sprintf(tmpfilename, "%s.tmp", check); //check.tmp

			if((fp1 = fopen(check, "r+")) == NULL){
				fprintf(stderr, "fopen error 1\n");
				exit(1);
			}

			fseek(fp1, 0, SEEK_END);
			size1 = ftell(fp1);
			fseek(fp1, 0, SEEK_SET);

			if(access(tmpfilename, F_OK) < 0){ //첫 동기화 파일이면 파일 지움
				remove(check);
				exit(0);
			}
			else{ //파일 이름 같고 파일 내용 바뀌었을 때
				struct stat tstat;
				if(stat(tmpfilename, &tstat) < 0){
					fprintf(stderr, "stat error tstat\n");
					exit(1);
				}

				mtime = tstat.st_mtime; //tmp파일의 최종 수정시간
				timebuf2.modtime = mtime;

				remove(check); //동기화 파일 삭제

				if(rename(tmpfilename, check) < 0){ //tmp파일을 동기화 파일 이름으로 변경
					fprintf(stderr, "rename error\n");
					exit(1);
				}
				
				fclose(fp1);
			
				if(utime(check, &timebuf2) < 0){ //최종 수정시간 설정
					fprintf(stderr, "utime error\n");
					exit(1);
				}
			}	
	
		} 
	} 
}
