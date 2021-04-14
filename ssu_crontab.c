#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>
#include <time.h>

#define SEC_TO_MICRO 1000000
#define BUFFER_SIZE 1024

struct timeval begin_t, end_t;

int daemon_init(void);
void promptmake(void);
void ssu_runtime(void);
int do_add(FILE *fp, char** command);
int do_remove(char** command);
int check_range(int num, int test);
void addlog(char** command);
void removelog(char* tmp);

int main(){
	pid_t daemonpid;
	FILE *fp;

	gettimeofday(&begin_t, NULL);
	
	if((fp = fopen("ssu_crontab_file", "a+")) == NULL){
		fprintf(stderr, "fopen error ssu_crontab_file\n");
		exit(1);
	}
	promptmake();
}

void promptmake(){
	char order[BUFFER_SIZE];
	char buf[BUFFER_SIZE];
	char strTemp[BUFFER_SIZE];
	char addnum[BUFFER_SIZE];
	char* test;
	char* ptr;
	char first[10], buffer[30];
	char** command = (char**)malloc(sizeof(char*)*7);
	int n=0, i=0, checkstr = 0;
	int re1, re2, checkcm1, checkcm2;
	FILE *fp;

	if(access("ssu_crontab_file", F_OK) == 0){ //ssu_crontab_file 존재할 때
		if((fp = fopen("ssu_crontab_file", "r+")) == NULL){
			fprintf(stderr, "fopen error\n");
			exit(1);
		}
	}
	else if(access("ssu_crontab_file", F_OK) < 0){//ssu_crontab_file 존재하지 않을 때
		if((fp = fopen("ssu_crontab_file", "w+")) == NULL){
			fprintf(stderr, "fopen error\n");
			exit(1);
		}
	}
	
	while(1){
		fp = fopen("ssu_crontab_file", "r+");
		fseek(fp, 0, SEEK_SET);
		checkstr = 0;
		while(fgets(strTemp, sizeof(strTemp), fp) != NULL){ //파일 한줄씩 읽으면서
			strTemp[strlen(strTemp) - 1] = '\0'; //개행문자 제거
			memset(addnum, 0, sizeof(addnum));
			sprintf(addnum, "%d. %s", checkstr, strTemp); 
			printf("%s\n", addnum); //파일에 등록된 명령어 출력
			checkstr++; //한줄마다 증가
		}

		memset(order, 0, 500);
		printf("\n20182595>"); //프롬프트 출력
		fgets(order, BUFFER_SIZE, stdin); //입력 받음
		order[strlen(order)-1] = '\0'; //개행제거

		if(!strncmp(order, "\0", 1)){ //엔터 입력시 프롬프트 재출력
			continue;
		}

		if(!strcmp(order, "exit")){ //exit 입력시 종료
			gettimeofday(&end_t, NULL);
			ssu_runtime();
			exit(1);
		}

		memcpy(buf, order, sizeof(order));
		ptr = strtok(order, " ");
		strcpy(first, ptr); //프롬프트 명령어

		if((strcmp(first, "add")) && (strcmp(first, "remove"))){ //주어진 명령어가 아닌 다른 명령어 입력시
			fprintf(stderr, "input command add or remove\n");
			continue;
		}

		if(!strcmp(first, "add")){ //add 명령어일 때
			for(int i = 0 ; i < 7 ; i++){ //5개의 주기 + 명령어 할당
				command[i] = (char*)malloc(sizeof(char)*30);
			}
			//입력 받은  명령어 잘라서 할당한 공간에 복사
			ptr = strtok(buf, " "); 
		
			for(i = 0 ; ; i++){ //명령어까지만 받음
				strcpy(command[i], ptr);
		
				ptr = strtok(NULL, " ");

				if(i==4){ //명령어까지 받았을 때
					strcpy(command[i+1], ptr);
					ptr = strtok(NULL, "\n");
					strcpy(command[i+2], ptr);
					break;
				}
			}

			re1 = do_add(fp, command); 
			if(re1 == 0) //옳지 않은 문법 => 프롬프트 재출력
				continue;
		}
		else if(!strcmp(first, "remove")){ //remove명령어일 때
			for(int i = 0 ; i < 2 ; i++){ //COMMAND_NUMBER 공간 할당
				command[i] = (char*)malloc(sizeof(char)*30);
			}
			
			ptr = strtok(buf, " ");
			for(int i = 0 ; ptr != NULL ; i++){
				command[i] = ptr;
				ptr = strtok(NULL, " ");
			}
			re2 = do_remove(command);
			if(re2 == 0) //옳지 않은 주기 => 프롬프트 재출력
				continue;
		}
	}
}

void addlog(char** command){ //add 프롬프트 명령 로그에 기록
	FILE *fp;
	time_t now_time;
	struct tm *now_date;
	char buf[100];
	time(&now_time);

	if((fp = fopen("ssu_crontab_log", "a+")) == NULL){
		fprintf(stderr, "fopen error\n");
		exit(1);
	}

	now_date = localtime(&now_time);
	sprintf(buf, "[%s ", asctime(now_date));
	buf[strlen(buf)-2] = '\0';
	strcat(buf, "]");
	for(int i = 0 ; i < 7 ; i++){
		strcat(buf, " ");
		strcat(buf, command[i]);
	}
	strcat(buf, "\n");
	fwrite(buf, strlen(buf), 1, fp);

	fclose(fp);
}	

void removelog(char* tmp){ //remove 프롬프트 명령 로그에 기록
	FILE *fp;
	time_t now_time;
	struct tm *now_date;
	char buf[100];
	time(&now_time);

	if((fp = fopen("ssu_crontab_log", "a+")) == NULL){
		fprintf(stderr, "fopen error\n");
		exit(1);
	}

	now_date = localtime(&now_time);
	sprintf(buf, "[%s ", asctime(now_date));
	buf[strlen(buf)-2] = '\0';
	strcat(buf, "] ");
	strcat(buf, "remove ");
	strcat(buf, tmp);
	fwrite(buf, strlen(buf), 1, fp);

	fclose(fp);
}	

int do_add(FILE *fp, char** command){ //add 명령어 실행
	int j = 0;
	int i, k;
	int num1, num2, beforenum;
	int re, check;
	int bnum, anum;
	int err=0;
	int count = 0, star=0, hyphen=0, comma=0, slash=0;
	char addbuf[BUFFER_SIZE];
	char addnum[BUFFER_SIZE];

	for(i = 1 ; i < 6 ; i++){ 
		count = 0, star = 0, hyphen = 0, comma=0, slash=0;

		if((command[i][0]=='*') || (isdigit(command[i][0]))){ //각 주기의 시작문자가 * 혹은 숫자일 때

			if(isdigit(command[i][0])){ //숫자로 시작
				num1 = atoi(&command[i][0]);
				re = check_range(i, num1); //범위 체크
				if((re = check_range(i, num1)) == 0){ //올바르지 않는 범위
					fprintf(stderr, "not within range\n");
					return 0;
				}
				if(i == 5){
					if(command[i][1] == '\0') //범위에 맞는 숫자일 때
						continue;
				}
				else if(i==1 || i ==2 || i == 3 || i ==4){
					if(command[i][2] == '\0') //숫자 뒤에 기호 없는 경우
						continue;
				}
			}
			else if(command[i][0] == '*'){ // *로 시작
				if(command[i][1] == '\0') // * 하나만 입력 가능
					continue;
			}

			for(k = 1 ; ; k++){ //각 주기 명령어 한글자씩 확인
				if(command[i][k] == '\0') //모두 확인하면 for문 나감
					break;
				if(isdigit(command[i][k])){ //** 숫자 **
					num2 = atoi(&command[i][k]);
					if((re = check_range(i, num2)) == 0){ //범위 벗어나면 에러
						fprintf(stderr, "not within range\n");
						return 0;
					}
					count++;

					if(isdigit(command[i][k-1])){ //두자리수
						num2 = atoi(&command[i][k-1]);
						if(num2 >= 10)
							continue;
					}
					if(command[i][k-1] != ',' && command[i][k-1] != '-' && command[i][k-1]!='/'){ //숫자 앞에 , - / 가 아니면 에러
						fprintf(stderr, "number before error\n");
						return 0;
					}
					if(command[i][k-1] == '-' && isdigit(command[i][k-2])){ //<숫자-현재숫자> 일 때 현재숫자가 더 작으면 에러
						beforenum = atoi(&command[i][k-2]);
						if(num2 < beforenum){
							fprintf(stderr, "wrong range\n");
							return 0;
						}
					}
				}
				else if(command[i][k] == '-'){ //** - **
					hyphen++;
					//앞뒤에 주기 오면 안됨
					if(command[i][k+1] == '\0'){ // - 만 입력받았을 때
						fprintf(stderr, "- next blank error\n");
						return 0;
					}
					if(!isdigit(command[i][k-1])){ //바로 앞에 숫자 아니면 에러
						fprintf(stderr, "- before error\n");
						return 0;
					}
					if(comma == 0 && hyphen > 1){ // <현재숫자 - > 나오기 전에 (-)가 나온 적이 있으면 에러 => 중복 사용
						fprintf(stderr, "- error 1\n");
						return 0;
					}
					if(count!=0 && slash!=0){ //(현재기호 -) 나오기 전에 숫자와 (/)가 나온 적이 있으면 에러 => 주기이므로
						fprintf(stderr, "- error2\n");
						return 0;
					}
				}
				else if(command[i][k] == ','){ //** , **
					comma++;
					slash = 0, hyphen = 0, count ==0;
					if(!isdigit(command[i][k-1]) && command[i][k-1] != '*'){ //바로 앞에 숫자나 '*' 아니면 에러
						fprintf(stderr, ", before error\n");
						return 0;
					}
					else if(command[i][k+1] == '\0'){ // , 하나만 입력하면 에러
						fprintf(stderr, ", next blank error\n");
						return 0;
					}
				}
				else if(command[i][k] == '/'){ //** / **
					slash++;
					if(command[i][k-1] != '*' && !isdigit(command[i][k-1])){ //바로 앞에 숫자나 '*' 아니면 에러
						fprintf(stderr, "/ error\n");
						return 0;
					}
					else if(command[i][k+1] == '\0'){ // / 하나만 입력하면 에러
						fprintf(stderr, "/ next blank error\n");
						return 0;
					}
					else if(isdigit(command[i][k-1]) && count < 2){ // <숫자/> 이전에 숫자가 한번만 나왔으면 에러
							fprintf(stderr, "/ error\n");
							return 0;
						}
					else if(isdigit(command[i][k-1]) && isdigit(command[i][k+1])){ // <숫자/숫자>
						bnum = atoi(&command[i][k-1]);
						anum = atoi(&command[i][k+1]);
						if(bnum < anum){ //뒤 숫자가 더 크다면 에러
							if(isdigit(command[i][k-2])) //두자리수
								continue;
							fprintf(stderr, "/ error5\n");
							return 0;
						}
					}
					else if(isdigit(command[i][k-1]) && hyphen == 0){ //바로 앞이 숫자인데 이전에 '-'이 나온 적 없으면 에러 => 주기 아님
						fprintf(stderr, "/error 1\n");
						return 0;
					}
					else if(command[i][k-1]!='*' && hyphen==0){ //바로 앞이 *이 아니고 이전에 '-'이 나온 적 없으면 에러
						fprintf(stderr, "/ error2\n");
						return 0;
					}
					else if(hyphen!=0 && slash > 1){ // 이전에 '-' 나온 적 있고 '/' 1개 이상이면 에러 => 중복사용
						fprintf(stderr, "/ error3\n");
						return 0;
					}
				}
				else if(command[i][k] == '*'){  // *
					star++;
					if(command[i][k-1] != ','){ //바로 앞에 ,가 아니면 에러
						fprintf(stderr, "* before error\n");
						return 0;
					}
				} 
				else{ //주어진 기호 외에 다른 기호면 에러
					fprintf(stderr, "unknown mark\n");
					return 0;
				}		
			}
		}
		else{ //*이나 숫자로 시작하지 않으면 에러
			fprintf(stderr, "start error\n");
			return 0;
		}
	} //for

	//입력 받은 주기 명령어가 올바르다면
	memset(addbuf, 0, BUFFER_SIZE);
	strcpy(addbuf, command[1]);
	for(int i = 2 ; i < 7 ; i++){
		strcat(addbuf, " ");
		strcat(addbuf, command[i]);
	}
	strcat(addbuf, "\n");

	fseek(fp, 0, SEEK_END);

	if(fwrite(addbuf, strlen(addbuf), 1, fp) != 1){ //ssu_crontab_file 파일에 씀
		fprintf(stderr, "fwrite error\n");
		exit(1);
	}

	fclose(fp);
	addlog(command); //add log 작성
}

int do_remove(char** command){
	int offset = 0;
	int num;
	char buf[BUFFER_SIZE];
	char* tmp = (char*)malloc(sizeof(char)*BUFFER_SIZE);
	FILE *fp1;
	FILE *fp2;

	if(!isdigit(command[1][0])){ // COMMAND_NUMBER가 숫자가 아닐 때
		fprintf(stderr, "input COMMAND_NUMBER!\n");
		return 0;
	}

	num = atoi(command[1]); //정수로 변환
	
	if(!strcmp(command[1],"\0")){ //COMMAND_NUMBER 입력하지 않았을 때
		fprintf(stderr, "input COMMAND_NUMBER!\n");
		return 0;
	}

	if(num < 0){ //잘못된 COMMAND_NUMBER
		fprintf(stderr, "number not exist\n");
		return 0;
	}

	if(access("ssu_crontab_file", F_OK) < 0){ 
		fprintf(stderr, "ssu_crontab_file doesn't exist\n");
		return 0;
	}

	if((fp1 = fopen("ssu_crontab_file", "r+")) == NULL){ //원본파일
		fprintf(stderr, "fopen error for ssu_crontab_file\n");
		exit(1);
	}
		
	if((fp2 = fopen("tmpfile", "w+")) == NULL){ //임시파일
		fprintf(stderr, "fopen error for tmpfile\n");
		exit(1);
	}
	
	fseek(fp1, 0, SEEK_SET);
	for(int i = 0 ; i < num ; i++){ //삭제 번호 이전까지 원본파일에서 한줄씩 읽고 임시파일에 복사
		memset(buf, 0, BUFFER_SIZE);
		fgets(buf, BUFFER_SIZE, fp1);
		fwrite(buf, strlen(buf), 1, fp2);
	}
	memset(buf, 0, BUFFER_SIZE);
	memset(tmp, 0, BUFFER_SIZE);
	fgets(buf, BUFFER_SIZE, fp1); //삭제할 명령어
	
	strcpy(tmp, buf);
	if(strlen(buf) == 0){ //COMMAND_NUMBER가 옳지 않을 때
		fprintf(stderr, "COMMAND NUMBER doesn't exist\n");
		return 0;
	}

	memset(buf, 0, BUFFER_SIZE);
	while(fgets(buf, BUFFER_SIZE, fp1) != NULL) //삭제 번호 이후부터 원본파일에서 한줄씩 읽고 임시파일에 복사
		if(fwrite(buf, strlen(buf), 1, fp2) != 1){
			fprintf(stderr, "fwrite error\n");
			exit(1);
		}

	remove("ssu_crontab_file"); //원본파일 삭제
	rename("tmpfile", "ssu_crontab_file"); //삭제 적용된 임시파일 이름 변경
	
	fclose(fp1);
	fclose(fp2);

	removelog(tmp); //삭제로그 기록
}

int check_range(int num, int test){ //입력 숫자의 범위 체크
	if(num == 1){ //분 (0-59)
		if(0 > test || test > 59){
			fprintf(stderr, "input error %d-1\n", num);
			return 0;
		}
		else
			return 1;
	}
	else if(num==2){ //시 (0-23)
		if(test < 0 || test > 23){
			fprintf(stderr, "input error %d-2\n", num);
			return 0;
		}
		else 
			return 1;
		}
	else if(num==3){ //일 (1-31)
		printf("test : %d\n", test);
		if(test < 1 || test > 31){
			fprintf(stderr, "input error %d-3\n", num);
			return 0;
		}
		else 
			return 1;
		}
	else if(num==4){ //월 (1-12)
		if(test < 1 || test > 12){
			fprintf(stderr, "input error %d-4\n", num);
			return 0;
		}
		else 
			return 1;
		}
	else if(num==5){ // 요일 (0-6)
		if(test < 0 || test > 6){
			fprintf(stderr, "input error %d-5\n", num);
			return 0;
		}
		else 
			return 1;
		}

}

void ssu_runtime(){
	end_t.tv_sec -= begin_t.tv_sec;

	if(end_t.tv_usec < begin_t.tv_usec){
		end_t.tv_sec--;
		end_t.tv_usec += SEC_TO_MICRO;
	}

	end_t.tv_usec -= begin_t.tv_usec;

	printf("Runtime %ld:%06ld(sec:usec)\n", end_t.tv_sec, end_t.tv_usec); //print runtime
}
