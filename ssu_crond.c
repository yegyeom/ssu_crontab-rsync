#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#define SEC_TO_MICRO 1000000
#define BUFFER_SIZE 1024

int daemon_init(void);
void check_command(char** command);
int check_range1(int i, int dnum, struct tm);
int check_range2(int i, int num, struct tm);
void mark_log(char** command);

int main() {
	FILE* fp;
	char* ptr;
	char buf[BUFFER_SIZE], tmp[BUFFER_SIZE];
	char** command = (char**)malloc(sizeof(char*) * 6);
	int i;
	time_t nowtime;
	struct tm tm;

	if(daemon_init() < 0){ //데몬 프로세스 실행
			fprintf(stderr, "daemon_init failed\n");
			exit(1);
	}

	while(1) {
		nowtime = time(NULL);
		tm = *localtime(&nowtime);
		if ((fp = fopen("ssu_crontab_file", "r+")) == NULL) { //ssu_crontab_file 오픈
			fprintf(stderr, "fopen error for ssu_crontab_file\n");
			exit(1);
		}
		while(!feof(fp)){
			if (fgets(tmp, BUFFER_SIZE, fp) != NULL){  //ssu_crontab_file 에서 한문장씩 읽어오고 자름
				for (int i = 0; i < 6; i++) { // 주기 + 명령어 각각 공간 할당해주고 내용 복사
					command[i] = (char*)malloc(sizeof(char) * 30);
				}
				tmp[strlen(tmp) - 1] = '\0';
				strcpy(buf, tmp);
				ptr = strtok(buf, " ");

				for (int i = 0; ; i++) {
					strcpy(command[i], ptr);
					ptr = strtok(NULL, " ");
	
					if (i == 3) {
						strcpy(command[i + 1], ptr);
						ptr = strtok(NULL, "\n");
						strcpy(command[i + 2], ptr);
						break;
					}
				}
				if(tm.tm_sec == 0){ //0초일 때 실행
					check_command(command);
				}
			}
		}
		sleep(1);
		rewind(fp); //파일 맨 앞으로
	
		fclose(fp);
	}
}

void check_command(char** command) {
	time_t nowtime = time(NULL);
	struct tm tm = *localtime(&nowtime);
	int min, hour, day, month, wday;
	int count = 0, star = 0, hyphen = 0, slash = 0, comma = 0, check=0;
	int arr[BUFFER_SIZE];
	int num1, bnum1;
	int num2, bnum2, anum2;
	int num3, bnum3, anum3;
	int num4, re;
	int re1, re2, rnum;
	int cnt=0;
	char test[30];
	char* ptr;
	
	for (int i = 0; i < 5; i++) { //주기 5개
		count = 0, star = 0, hyphen = 0, slash = 0, comma = 0;
		cnt = 0;
		memset(arr, 0, BUFFER_SIZE);

		for (int j = 0; ; j++) { //각 주기의 내부
			//기호, 숫자 나온 횟수 체크
			if (command[i][j] == '\0')
				break;
			if (command[i][j] == '*')
				star++;
			else if (command[i][j] == '-')
				hyphen++;
			else if (command[i][j] == '/')
				slash++;
			else if (command[i][j] == ',')
				comma++;
			else if (isdigit(command[i][j])) {
				if (isdigit(command[i][j - 1])) {
					bnum1 = atoi(&command[i][j - 1]);
					if (bnum1 >= 10) //두자리수
						continue;
					else {
						fprintf(stderr, "number error\n");
						exit(1);
					}
				}
				num1 = atoi(&command[i][j]);
				count++;
			}
		} 
		
		if (star != 0 && count == 0) { // '*'
			check++;
			continue;
		}
		else if (star == 0 && count != 0) { // * 없이 숫자만
			if (comma == 0) { // 주기에 , 없을 때
				if (hyphen == 0 && slash == 0) { //기호 없이 숫자만 있을 때
					if((rnum = check_range2(i, num1, tm)) == 1){ //범위체크
						check++;
						continue;
					}
				}
				else if (slash != 0) { // bnum2-num2/anum2
					bnum2 = atoi(&command[i][0]);

					if (isdigit(command[i][1]))  //두자리수
						num2 = atoi(&command[i][3]);
					else if(!isdigit(command[i][1])) //한자리수
						num2 = atoi(&command[i][2]);

					strcpy(test, command[i]);
					ptr = strrchr(test, '/');
					anum2 = atoi(&command[i][ptr - test + 1]); //나눌 숫자

					for (int k = bnum2-1; k <= num2; k+=anum2) {
						if(k == bnum2 -1)
							continue;
						if((rnum = check_range2(i, k, tm)) == 1){ //범위체크
							check++;
							re1 = 1;
							break;
						}
					}
					if(re1 == 1) //현재 시각과 일치한다면 다음 항목 검사
						continue;
				}
				else if (slash == 0) { // 숫자 - 
					if (hyphen != 1) {
						fprintf(stderr, "- overlap error 1\n");
						exit(1);
					}
					else { // 숫자 - 숫자 => bnum2 - num2
						bnum2 = atoi(&command[i][0]);

						if (isdigit(command[i][1])) { //두자리수
							num2 = atoi(&command[i][3]);
						}
						else if(!isdigit(command[i][1])){ //한자리수
							num2 = atoi(&command[i][2]);
						}
						for (int k = bnum2; k <= num2 ; k++) {
							if((rnum = check_range2(i, k, tm)) == 1){ //범위체크
								re1 = 1;
								check++;
								break;
							}
						} 
						if(re1 == 1) //현재 시각과 일치하다면 다음 항목 검사
							continue;
					} 
				} 

			} 
			else if (comma != 0) { // 주기에 , 있을 때
				count = 0, slash = 0, hyphen = 0, comma = 0, cnt = 0;
				for (int k = 0; ; k++) { //한 개의 주기 돌면서
					if (isdigit(command[i][k])) { //숫자일 때
						if (isdigit(command[i][k - 1])) { //두자리수
							num3 = atoi(&command[i][k - 1]);
							if (num3 >= 10)
								continue;
						}
						num3 = atoi(&command[i][k]);
						arr[cnt] = num3; //배열에 넣어줌
						cnt++; //숫자 하나 넣어줄 때마다 cnt 증가
					}
					else if (command[i][k] == '-')
						hyphen++;
					else if (command[i][k] == '/')
						slash++;
					else if(command[i][k] == ',')
						comma++;

					if (command[i][k] == ',' || (command[i][k] == '\0' && comma >= 1)) {
						if (hyphen != 0 && slash != 0) { // 숫자-숫자/숫자
							for (int m = arr[cnt - 3] - 1; m <= arr[cnt - 2]; m += arr[cnt - 1]) { 
								if(m == arr[cnt-3]-1)
									continue;
								if((rnum = check_range2(i, m, tm)) == 1){ //범위체크
									check++;
									re2 = 1;
									break;
								}
							}
							if(re2 == 1){ //현재 시각과 일치하다면 다음 항목 검사
								re1 = 1;
								break;
							}
							cnt = cnt - (hyphen + slash) - 1; //배열 추가 위치 변경
						}
						else if (hyphen != 0 && slash == 0) { //숫자 - 숫자
							for (int m = arr[cnt - 2]; m <= arr[cnt - 1]; m++) {
								if((rnum = check_range2(i, m, tm)) == 1){ //범위체크
									check++;
									re2 = 1;
									break;
								}
							}
							if(re2 == 1){
								re1 = 1;
								break;
							}
							cnt = cnt - (hyphen + slash) - 1; //배열 추가 위치 변경
						}
						else{ //숫자
							if((rnum = check_range2(i, arr[cnt-1], tm)) == 1){ //범위체크
								check++;
								re2 = 1;
								break;
							}
						}
						slash = 0, hyphen = 0, count = 0;
					}

					if (command[i][k] == '\0')
						break;
				}

				if(re1 == 1) //현재 시각과 일치하다면 다음 항목 검사
					continue;

				for (int k = 0; k < cnt ; k++) { //숫자만 담아 놓은 배열 검사
					if((rnum = check_range2(i, arr[k], tm)) == 1){ //범위체크
						check++;
						re1 = 1;
						break;
					}
				}

				if(re1 == 1) //현재 시각과 일치하다면 다음 항목 검사
					continue;
			}
		}
		else if (star != 0 && count != 0) { //* 숫자 * / - ,
			if (comma == 0) { // 콤마 없을 때 => */숫자
				if (hyphen != 0) {
					fprintf(stderr, "errror */num");
					exit(1);
				}

				strcpy(test, command[i]);
				ptr = strrchr(test, '/'); // 마지막으로 나온 / 위치
				num4 = atoi(&command[i][ptr - test + 1]); //나눌 숫자

				if ((re = check_range1(i, num4, tm)) == 1){ //범위체크
					check++;
					continue;
				}
			}
			else if (comma != 0) { //콤마를 기준으로 모든 경우 연결되어 있음
				count=0, slash=0,hyphen=0, comma=0, star =0, cnt=0;

				for(int k = 0 ; ; k++){ //한개의 주기를 돌면서
					if(isdigit(command[i][k])){ //숫자 만나면 배열에 넣어줌
						if(isdigit(command[i][k-1])){ //두자리수
							num3 = atoi(&command[i][k-1]);
							if(num3 >= 10)
								continue;
						}
						count++;
						num3 = atoi(&command[i][k]);
						arr[cnt] = num3; //배열에 추가
						cnt++;
					}
					else if(command[i][k] == '*'){ // * 만나면 배열에 넣어줌
						star++;
						arr[cnt] = -1;
						cnt++;
					}
					else if(command[i][k] == '-')
						hyphen++;
					else if(command[i][k] == '/')
						slash++;
					else if(command[i][k] == ',')
						comma++;
					
					if(command[i][k] == ',' || ((command[i][k] == '\0') && comma>=1)){ //주기 내에서 콤마 기준으로
						if(star!=0 && count==0){ // * 
							check++;
							re1 = 1;
							break;
							//나머지 콤마 코드 읽을 필요 X
						}
						else if(star == 0 && count!=0){ //* 없음 
							if(hyphen == 0 && slash ==0){ //숫자만
								if((rnum = check_range2(i, arr[cnt-1], tm)) == 1){ //범위체크
									check++;
									re1 = 1;
									break;
								}	
							}
							else if(hyphen != 0 && slash != 0){ // 숫자-숫자/숫자
								for(int k = arr[cnt-3]-1 ; k <= arr[cnt-2] ; k+=arr[cnt-1]){
									if(k == arr[cnt-3]-1)
										continue;
									if((rnum = check_range2(i, k, tm)) == 1 ){ //주기체크
										check++;
										re2 = 1;
										break;
									}
								}
								if(re2 == 1){
									re1 = 1;
									break;
								}
							}
							else if(hyphen!=0 && slash ==0){ //숫자-숫자
								for(int k = arr[cnt-2] ; k <= arr[cnt-1] ; k++){
									if((rnum = check_range2(i, k, tm)) == 1){ //범위체크
										check++;
										re2 = 1;
										break;
									}
								}
								if(re2 == 1){
									re1 = 1;
									break;
								}
							}
							//여기 범위에서 맞으면 끝 아니면 다름 콤마 코드
						}
						else if(star!=0 && count!=0){ // */숫자
							if((rnum = check_range1(i, arr[cnt-1], tm)) == 1){
								check++;
								re1 = 1;
								break;
							}
							//여기 범위에서 맞으면 끝 아니면 다른 콤마 코드
						}
						
						//변수 초기화해주고 다음 콤마 코드 처음부터 읽기
						star = 0, hyphen = 0, slash = 0, count=0;
					}

					if(command[i][k] == '\0')
						break;
				} //for
				if(re1 == 1) //현재 시각과 일치하면 다음 항목 검사
					continue;
			} //comma!=0
		}
		
	} //for 1

	if(check == 5){ //모든 주기 항목이 현재 시간과 일치
		if(system(command[5]) < 0){ //명령어 실행
			fprintf(stderr, "system error\n");
			return;
		}
		mark_log(command); //명령어 실행 로그
	}
}

void mark_log(char** command){ //명령어 실행시 로그
	FILE *fp;
	time_t now_time;
	struct tm *now_date;
	char buf[100];
	time(&now_time);

	if((fp = fopen("ssu_crontab_log", "a+")) == NULL){
		fprintf(stderr, "fopen error for ssu_crontab_log\n");
		exit(1);
	}
	
	now_date = localtime(&now_time);
	sprintf(buf, "[%s ", asctime(now_date));
	buf[strlen(buf)-2] = '\0';
	strcat(buf, "] run");

	for(int i = 0 ; i < 6 ; i++){
		strcat(buf, " ");
		strcat(buf, command[i]);
	}

	strcat(buf, "\n");
	fwrite(buf, strlen(buf), 1, fp);
	
	fclose(fp);
}

int check_range1(int i, int dnum, struct tm tm) { 
	//주어진 숫자가 각 주기항목에 알맞은 범위인지 체크
	if (i == 0) { // 분
		for (int k = -1; k <= 59; k += dnum) {
			if (k == tm.tm_min)
				return 1;
		}
	}
	else if (i == 1) { // 시
		for(int k = -1 ; k <= 23 ; k+=dnum){
			if (tm.tm_hour == k) 
				return 1;
		}
	}
	else if (i == 2) { // 일
		for (int k = 0; k <= 31; k += dnum) {
			if (tm.tm_mday == k)
				return 1;
		}
	}
	else if (i == 3) { // 월
		for (int k = 0; k <= 12; k += dnum) {
			if (tm.tm_mon + 1 == k)
				return 1;
		}
	}
	else if (i == 4) { //요일
		for (int k = -1; k <= 6; k += dnum) {
			if (tm.tm_wday == k)
				return 1;
		}
	}
}

int check_range2(int i, int num, struct tm tm){
	//주어진 숫자가 현재 시각과 일치한지 체크
	if (i == 0) { //분
		if (tm.tm_min == num){
			return 1;
		}
	}
	else if (i == 1) { //시
		if (tm.tm_hour == num){
			return 1;
		}
	}
	else if (i == 2) { //일
		if (tm.tm_mday == num){
			return 1;
		}
	}
	else if (i == 3) { //월
		if (tm.tm_mon+1 == num){
			return 1;
		}
	}
	else if (i == 4) { //요일
		if (tm.tm_wday == num){
			return 1;
		}
	}
}

int daemon_init(void) { //데몬 프로세스 생성
	pid_t pid;
	int fd, maxfd;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if (pid != 0)
		exit(0);

	pid = getpid();
	setsid();
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	maxfd = getdtablesize();

	for (fd = 0; fd < maxfd; fd++)
		close(fd);

	umask(0);
	
	if((fd = open("/dev/null", O_RDWR)) < 0){
		fprintf(stderr, "open error dev.null\n");
		exit(1);
	}
	dup(0);
	dup(0);
	return 0;
}
