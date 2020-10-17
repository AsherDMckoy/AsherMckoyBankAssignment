#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "bank.h"
#include "command.h"
#include "errors.h"


static int *accounts = NULL;


static int account_count = 0;


static int atm_count = 0;


int *get_accounts () {
  return accounts;
}




static int check_pipe_write (int result) {
  if (result < 0) {
    error_msg(ERR_PIPE_WRITE_ERR,
              "could not write to atm output file descriptor");
    return ERR_PIPE_WRITE_ERR;
  } else if (result < MESSAGE_SIZE) {
    error_msg(ERR_PIPE_WRITE_ERR,
              "partial message written to atm output file descriptor");
    return ERR_PIPE_WRITE_ERR;
  } else {
    return SUCCESS;
  }
}



static int check_pipe_read (int result) {
  if (result < 0) {
    error_msg(ERR_PIPE_READ_ERR,
              "could not read from bank input file descriptor");
    return ERR_PIPE_READ_ERR;
  } else if (result < MESSAGE_SIZE) {
    error_msg(ERR_PIPE_READ_ERR,
              "partial message read from bank input file descriptor");
    return ERR_PIPE_READ_ERR;
  } else {
    return SUCCESS;
  }
}



static int check_valid_atm (int atmid) {
  if (0 <= atmid && atmid < atm_count) {
    return SUCCESS;
  } else {
    error_msg(ERR_UNKNOWN_ATM, "message received from unknown ATM");
    return ERR_UNKNOWN_ATM;
  }
}




static int check_valid_account (int accountid) {
  if (0 <= accountid && accountid < account_count) {
    return SUCCESS;
  } else {
    error_msg(ERR_UNKNOWN_ACCOUNT, "message received for unknown account");
    return ERR_UNKNOWN_ACCOUNT;
  }
}




void bank_open (int atm_cnt, int account_cnt) {
  atm_count = atm_cnt;
  accounts = (int *)malloc(sizeof(int) * account_cnt);
  for (int i = 0; i < account_cnt; i++) {
    accounts[i] = 0;
  }
  account_count = account_cnt;
}




void bank_close () {
  free(accounts);
}




void bank_dump () {
  for (int i = 0; i < account_count; i++) {
    printf("Account %d: %d\n", i, accounts[i]);
  }
}


int bank (int atm_out_fd[], Command *cmd, int *atm_cnt) {
  cmd_t c;
  int i, f, t, a;
  Command bankcmd;

  cmd_unpack(cmd, &c, &i, &f, &t, &a);
  int result = SUCCESS;

  // TODO:
  // START YOUR IMPLEMENTATION

if(check_valid_atm(i) != SUCCESS)
   return ERR_UNKNOWN_ATM;

if(c == CONNECT)
{
    MSG_OK(&bankcmd,0,f,t,a);

    result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
   
}

if(c == EXIT) 
{
  (*atm_cnt) -= 1;

  MSG_OK(&bankcmd,0,f,t,a);

  result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
  
 }

if(c == DEPOSIT) 
{
    if(check_valid_account(t)==SUCCESS)
    {
      accounts[t]+=a;

      MSG_OK(&bankcmd,0,f,t,a);

      result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
    }
    else
    {
      MSG_ACCUNKN(&bankcmd,0,t);

      result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
    }

}

if(c == WITHDRAW)
{
  if(check_valid_account(f)==SUCCESS)
      {
          
	  if(accounts[f] >= a)
              {
		        accounts[f]-=a;

		        MSG_OK(&bankcmd,0,f,t,a);

		        result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
               }
           else
               {
		         MSG_NOFUNDS(&bankcmd,0,f,a);

		         result =check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
              }

       }
  else
    {

	    MSG_ACCUNKN(&bankcmd,0,t);

	    result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));

    }

}


if(c == TRANSFER)
{
 
  if(check_valid_account(f)!=SUCCESS)
      {
          MSG_ACCUNKN(&bankcmd,0,f);

          result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));

      }
  else if (check_valid_account(t)!=SUCCESS)
      {
          MSG_ACCUNKN(&bankcmd,0,t);

          result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
      }
  else 
     {
          if(accounts[f] < a)
                {
			  MSG_NOFUNDS(&bankcmd,0,f,a);

			  result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
	  
                 }
          else
             {
		      accounts[f] = accounts[f]-a;

		      accounts[t] = accounts[t]+a;

		      MSG_OK(&bankcmd,0,f,t,a);

		      result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));
      
            }

    }

} 


if(c == BALANCE)
{
  if(check_valid_account(f)!=SUCCESS)
      {
		MSG_ACCUNKN(&bankcmd,0,t);

		result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));   
      }
  else
     {
	      a = accounts[i];

	      MSG_OK(&bankcmd,0,f,t,a);

	      result = check_pipe_write(write(atm_out_fd[i],&bankcmd,MESSAGE_SIZE));	    
     }

}

if(c!= CONNECT && c != EXIT && c != DEPOSIT && c != TRANSFER && c != BALANCE && c != WITHDRAW)
  result =ERR_UNKNOWN_CMD;

  // END YOUR IMPLEMENTATION

  return result;
}


// This simply repeatedly tries to read another command from the
// bank input fd (coming from any of the atms) and calls the bank
// function to process the message and develop and send a reply.
// It stops when there are no active atms.

int run_bank (int bank_in_fd, int atm_out_fd[]) {
  Command cmd;

  int result  = 0;
  int atm_cnt = atm_count;
  while (atm_cnt != 0) {
    result = check_pipe_read(read(bank_in_fd, &cmd, MESSAGE_SIZE));
    if (result != SUCCESS)
      return result;

    result = bank(atm_out_fd, &cmd, &atm_cnt);

    if (result == ERR_UNKNOWN_ATM) {
      printf("received message from unknown ATM. Ignoring...\n");
      continue;
    }

    if (result != SUCCESS) {
      return result;
    }
  }

  return SUCCESS;
}
