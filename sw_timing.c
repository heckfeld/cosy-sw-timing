
/******************************************************************************/
/*
 */
/*
 * sw_timing.c 
 */
/*
 */
/******************************************************************************/

/****
      Schema:
      Konfiguration aus $TCL/conf/guis.conf auslesen und in giutab eintragen.

      Fuer jeden Eintrag Socktet 's' unter der Port-Nummer  oeffnen und
      in guitab  unter .reception_socket eintragen. 

      Bei Anmeldung einer Oberflaeche mit 'accept' unter dem Socket 
      .active_socket eintragen.
****/

#define EXTERN

#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>



#include "sw_timing.h"


extern int      errno;
int             debug = 0;
int             timer_stopped = 1;
FILE           *log_file_fd;

void            release_connection();
void            information();
void            sig_handling();
void            configure();
void            confirm();
void            clock_tick();

/** guitab[] enthalten die Namen, Server-Ports ,
    Adressen-Server-Port-Nummern und Zeiger auf den geoeffneten 
    Adressen-Server-Socket fuer die entsprechende Oberflaeche.
**/

int             s;
int             do_inform = 0;
int             tx_gui_socket = 0;
int             tx_udp_recv_port = 0;
int             tx_udp_send_port = 0;
int             tx_udp_recv_socket = 0;
int             tx_udp_send_socket = 0;
FILE           *gui_conf;
FILE           *gui_host;
FILE           *hidden_trigger_fd;
FILE           *hidden_inform_fd;
char            gui_conf_name[80];
char            hidden_trigger_list[80];
char            hidden_inform_list[80];

int             go = 0;		/* Start Stop */

unsigned long   sw_timing_pid;
FILE           *sw_timing_pid_file;
char            sw_timing_pid_name[80];
char            program_name[80];
char            sw_timing_host_name[80];
char            gui_host_file[80];

char           *set_format =
    "\"%[^\"] %*s \"%[^\"] %*s %d %d \"%[^\"] %*s %d";

char           *default_conf_dir = "/usr/local/tcl/conf";
char           *default_pid_dir = "/usr/local/tcl/etc";
char           *default_dir_name = "/usr/local/tcl/conf";
char           *default_conf_name = "guis.conf";
char           *default_hidden_trigger_list = ".sw_triggers";
char           *default_hidden_inform_list = ".sw_inform";
char           *default_host_name = "hosts";
char           *default_pid_name = "sw_timing.pid";
#define LINE_LEN 256

char            line[LINE_LEN];
char            gui_list[MAXMSG];
char            host_name[257];
int             host_node;
struct hostent *host_entry;
struct hostent  gethostbyXXX = { "unknown", NULL, AF_INET, 0, NULL };
unsigned long   my_inet_addr;
struct sockaddr_in tx_usta_addr_in;
struct sockaddr_in tx_usta_addr_out;

int             targ_ls;
int             addrlen,
                recv_addrlen,
                client_addrlen;
struct sockaddr_in client_addr;
struct sockaddr_in recv_addr;
int             tcpmask[MAX_SOCKETS];
#ifdef hpux
struct fd_set   readmask,
                readfds,
                excepfds;
#else
fd_set          readmask,
                readfds,
                excepfds;
#endif
int             nfound;

char            mtext[MAXMSG];
char           *ping = "PING\n";

GUITAB         *this_gui;
GUITAB         *this_inform = (GUITAB *) 0;
int             this_socket;

struct itimerval timer;
struct itimerval stp_timer;
unsigned long   intern_cycle_time = 0;
#define USECONDS 100000		/* Alle 100 ms tick */
unsigned long   usec = USECONDS;
unsigned long   interval = USECONDS / 1000;

/*
 * KEEP_ALIVE Timeout = 10 * alarm_intvl 
 */
unsigned int    is_et_su_widd = 0;
unsigned int    alarm_intvl = 75;

/*** Timing Sender Ustate-Meldung *******/
char            gui_name[80];
char            gui_cmd[80];
int             old_status = 2;
int             curm,
                curs,
                status = 0;	/* Byteweise */
int             eventcount,
                modus;		/* Byteweise */
int             cycle_duration;
int             cycle_number = 0;
int             last_cycle_number = -1;
int             cycle_time;
int             exp_wechsel;
int             exp_index;
int             last_exp_index;
int             exp_number;
int             last_exp_number;
#define MAX_EXP 9
int             poco_start[MAX_EXP + 1];
int             current_exp = 0;
int             lost_ticks = 0;

TRIGGER         trigger[2][MAX_TRIGGER + 1];
HIDDEN_LIST     hidden_trigger[MAX_TRIGGER + 1];
HIDDEN_LIST    *hidden_trigger_head = (HIDDEN_LIST *) 0;
int             hidden_count = 0;
TRIGGER         free_trigger_list[2];
TRIGGER         trigger_head[2];
TRIGGER_PTR     next_trigger;
TRIGGER_PTR     current_trigger_head;
long            tab_offset;
int             curr_trg_ptr;
int             swap_trg_ptr;
int             trg_tab_copied;
int             swap_trigger_table = 0;

INFORM          inform[MAX_INFORM + 1];
INFORM          free_inform_list;
INFORM          inform_head;
INFORM_PTR      next_inform;
struct sigaction sigact;

void
init_inform()
{
    int             i,
                    j,
                    k;
    char           *tmp;
    char            host[128];
    int             port;

    tx_udp_send_socket = create_send_socket(&tx_usta_addr_out);

    inform[0].next_inform = &inform[1];
    inform[0].prev_inform = (INFORM_PTR) 0;
    for (i = 1; i < MAX_INFORM; i++) {
	inform[i].next_inform = &inform[i + 1];
	inform[i].prev_inform = &inform[i - 1];
    }
    inform[MAX_INFORM].next_inform = 0;
    inform[MAX_INFORM].prev_inform = &inform[MAX_INFORM - 1];
    inform_head.next_inform = (INFORM_PTR) 0;
    free_inform_list.next_inform = &inform[0];

    /*
     * Inform clients aktivieren
     */
    hidden_inform_fd = fopen(hidden_inform_list, "r");
    if (hidden_inform_fd != 0) {
	while ((tmp =
		fgets(line, LINE_LEN, hidden_inform_fd)) != (char *) 0) {
	    if ((i = sscanf(line, "\"%[^\"] %*s  %d ", host, &port) == 2)) {
		set_inform(port, host);
	    }
	}
	fclose(hidden_inform_fd);
    }

}

INFORM_PTR
alloc_inform()
{
    int             i,
                    j,
                    k;
    INFORM_PTR      t0,
                    t1,
                    t2;

    t1 = free_inform_list.next_inform;
    if (t1) {
	free_inform_list.next_inform = t1->next_inform;
	if (t1->next_inform)
	    t1->next_inform->prev_inform = &free_inform_list;
    }
    return (t1);
}

void
free_inform(inf)
     INFORM_PTR      inf;
{
    int             i,
                    j,
                    k;
    INFORM_PTR      t0,
                    t1,
                    t2;

    t1 = free_inform_list.next_inform;
    inf->next_inform = t1;
    inf->prev_inform = &free_inform_list;
    free_inform_list.next_inform = inf;
    if (t1)
	t1->prev_inform = inf;
}

int
set_inform(port, host)
     char           *host;
     int             port;
{
    INFORM_PTR      inf,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found;
    struct hostent *hp;
    int             socket = tx_udp_send_socket;

    hp = gethostbyname(host);

    if ((inf = alloc_inform()) == (INFORM_PTR) 0)
	return (0);

    for (t = inform_head.next_inform, t_prev = &inform_head;
	 t != (INFORM_PTR) 0; t_prev = t, t = t->next_inform) {
	if ((t->port == port) && (strcmp(t->host, host) == 0)
	    ) {
	    return (0);
	}
    }

    inf->next_inform = t;
    inf->prev_inform = t_prev;
    t_prev->next_inform = inf;
    if (t)
	t->prev_inform = inf;

    sprintf(inf->host, "%s", host);
    inf->socket = socket;
    inf->send_addr.sin_addr.s_addr =
	((struct in_addr *) (hp->h_addr))->s_addr;
    inf->send_addr.sin_family = AF_INET;
    inf->send_addr.sin_port = htons(port);
    inf->port = port;
    return (1);
}

int
delete_inform(number)
     int             number;
{
    INFORM_PTR      inf,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found;

    for (found = 0, i = 1, t = inform_head.next_inform, t_prev =
	 &inform_head; t != (INFORM_PTR) 0;
	 t_prev = t, t = t->next_inform, i++) {
	if (i == number) {
	    found = i;
	    break;
	}
    }

    if (!found)
	return (0);

    t_prev->next_inform = t->next_inform;
    if (t->next_inform)
	t->next_inform->prev_inform = t_prev;

    free_inform(t);
    return (1);
}

void
rm_inform(port, host)
     char           *host;
     int             port;
{
    INFORM_PTR      trg,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found = 0;

    for (i = 1, t = inform_head.next_inform;
	 t != (INFORM_PTR) 0; t = t->next_inform, i++) {
	if ((t->port == port) && (strcmp(t->host, host) == 0)
	    ) {
	    delete_inform(i);
	}
    }
}

void
prompt(tmp)
     char           *tmp;
{
    int             socket = this_gui->active_socket;

    if (write(socket, tmp, strlen(tmp)) == -1) {
	if (errno != EINTR)
	    release_connection();
    }
}
void
list_inform()
{
    INFORM_PTR      trg,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found = 0;
    char            list[128 * 1024];
    char            tmp[512];
    int             socket = this_gui->active_socket;

    list[0] = '\0';

    for (i = 1, t = inform_head.next_inform;
	 t != (INFORM_PTR) 0; t = t->next_inform, i++) {
	sprintf(tmp, " {%s:%d}", t->host, t->port);
	strcat(list, tmp);
    }
    strcat(list, "\n");
    if (write(socket, list, strlen(list)) == -1) {
	if (errno != EINTR)
	    release_connection();
    }
}


void
clear_tab(j)
     int             j;
{
    int             i,
                    k;

    trigger[j][0].next_trigger = &trigger[j][1];
    trigger[j][0].prev_trigger = (TRIGGER_PTR) 0;
    for (i = 1; i < MAX_TRIGGER; i++) {
	trigger[j][i].next_trigger = &trigger[j][i + 1];
	trigger[j][i].prev_trigger = &trigger[j][i - 1];
    }
    trigger[j][MAX_TRIGGER].next_trigger = 0;
    trigger[j][MAX_TRIGGER].prev_trigger = &trigger[j][MAX_TRIGGER - 1];
    free_trigger_list[j].next_trigger = &trigger[j][0];
    trigger_head[j].next_trigger = (TRIGGER_PTR) 0;

    trg_tab_copied = 1;
}

void
clear_trigger_tab(j)
     int             j;
{
    clear_tab(swap_trg_ptr);
}

void
init_trigger()
{
    int             i,
                    j,
                    k;

    /*
     * swap_trigger_table = 0; 
     */
    curr_trg_ptr = 0;
    swap_trg_ptr = 1;

    for (j = 0; j < 2; j++) {
	clear_tab(j);
    }

    for (j = 0; j <= MAX_EXP; j++)
	poco_start[j] = 0;

    trg_tab_copied = 1;

    tab_offset = &trigger[1][0] - &trigger[0][0];
    current_trigger_head = &trigger_head[curr_trg_ptr];
    next_trigger = current_trigger_head;

    /*
     * hidden_trigger_head = (HIDDEN_LIST *) 0; 
     */
}

void
cp_trigger_table()
{
    int             i,
                    j,
                    k;

    TRIGGER_PTR     tc = &trigger[curr_trg_ptr][0];
    TRIGGER_PTR     ts = &trigger[swap_trg_ptr][0];
    TRIGGER_PTR     tch = &trigger_head[curr_trg_ptr];
    TRIGGER_PTR     tsh = &trigger_head[swap_trg_ptr];
    TRIGGER_PTR     tcf = &free_trigger_list[curr_trg_ptr];
    TRIGGER_PTR     tsf = &free_trigger_list[swap_trg_ptr];


    if (swap_trg_ptr > curr_trg_ptr) {
	for (i = 0; i <= MAX_TRIGGER; i++, ts++, tc++) {
	    if (tc->next_trigger)
		ts->next_trigger = tc->next_trigger + tab_offset;
	    else
		ts->next_trigger = (TRIGGER_PTR) 0;
	    if (tc->prev_trigger)
		ts->prev_trigger = tc->prev_trigger + tab_offset;
	    else
		ts->prev_trigger = (TRIGGER_PTR) 0;
	}
	if (tch->next_trigger) {
	    tsh->next_trigger = tch->next_trigger + tab_offset;
	    tsh->next_trigger->prev_trigger = tch;
	} else
	    tsh->next_trigger = (TRIGGER_PTR) 0;

	if (tcf->next_trigger) {
	    tsf->next_trigger = tcf->next_trigger + tab_offset;
	    tsf->next_trigger->prev_trigger = tcf;
	} else
	    tsf->next_trigger = (TRIGGER_PTR) 0;
    } else {
	for (i = 0; i <= MAX_TRIGGER; i++, ts++, tc++) {
	    if (tc->next_trigger)
		ts->next_trigger = tc->next_trigger - tab_offset;
	    else
		ts->next_trigger = (TRIGGER_PTR) 0;

	    if (tc->prev_trigger)
		ts->prev_trigger = tc->prev_trigger - tab_offset;
	    else
		ts->prev_trigger = (TRIGGER_PTR) 0;
	}
	if (tch->next_trigger) {
	    tsh->next_trigger = tch->next_trigger - tab_offset;
	    tsh->next_trigger->prev_trigger = tch;
	} else
	    tsh->next_trigger = (TRIGGER_PTR) 0;

	if (tcf->next_trigger) {
	    tsf->next_trigger = tcf->next_trigger - tab_offset;
	    tsf->next_trigger->prev_trigger = tcf;
	} else
	    tsf->next_trigger = (TRIGGER_PTR) 0;
    }

    for (tc = tch->next_trigger, ts = tsh->next_trigger;
	 tc; tc = tc->next_trigger, ts = ts->next_trigger) {
	ts->time = tc->time;
	ts->aflag = tc->aflag;
	ts->hidden = tc->hidden;
	ts->experiment_mask = tc->experiment_mask;
	ts->guitab_ptr = tc->guitab_ptr;
	sprintf(ts->name, "%s", tc->name);
	sprintf(ts->cmd, "%s", tc->cmd);
    }

    trigger[swap_trg_ptr][MAX_TRIGGER].next_trigger = 0;

    trg_tab_copied = 1;
}

int
list_trigger_tab()
{
    int             i,
                    j,
                    k;
    char            list[512 * 1024];
    char            tmp[512];

    TRIGGER_PTR     t;
    int             socket = this_gui->active_socket;

    if (!socket)
	return (0);

    if (!trg_tab_copied)
	cp_trigger_table();

    list[0] = '\0';

    for (t = trigger_head[swap_trg_ptr].next_trigger;
	 t; t = t->next_trigger) {
	if (!t->hidden) {
	    sprintf(tmp, " {%s:%s:%d:%d:%d:%s}",
		    t->name, t->guitab_ptr->name, t->time, t->aflag,
		    t->experiment_mask, t->cmd);
	    strcat(list, tmp);
	}
    }
    strcat(list, "\n");
    if (write(socket, list, strlen(list)) == -1) {
	if (errno != EINTR)
	    release_connection();
    }
    return (1);
}

TRIGGER_PTR
alloc_trigger()
{
    int             i,
                    j,
                    k;
    TRIGGER_PTR     t0,
                    t1,
                    t2;

    t1 = free_trigger_list[swap_trg_ptr].next_trigger;
    if (t1) {
	free_trigger_list[swap_trg_ptr].next_trigger = t1->next_trigger;
	if (t1->next_trigger)
	    t1->next_trigger->prev_trigger =
		&free_trigger_list[swap_trg_ptr];
    }
    return (t1);
}

void
free_trigger(trg)
     TRIGGER_PTR     trg;
{
    int             i,
                    j,
                    k;
    TRIGGER_PTR     t0,
                    t1,
                    t2;

    t1 = free_trigger_list[swap_trg_ptr].next_trigger;
    trg->next_trigger = t1;
    trg->prev_trigger = &free_trigger_list[swap_trg_ptr];
    free_trigger_list[swap_trg_ptr].next_trigger = trg;
    if (t1)
	t1->prev_trigger = trg;
}

int
set_trigger(name, time, gui, aflag, cmd, exp_mask, hidden)
     char           *name;
     unsigned int    time;
     char           *gui;
     int             aflag;
     char           *cmd;
     unsigned long   exp_mask;
     int             hidden;
{
    TRIGGER_PTR     trg,
                    t,
                    t_prev,
                    t_next;
    GUITAB         *th,
                   *found;
    int             i;

    if (!trg_tab_copied)
	cp_trigger_table();

    for (found = (GUITAB *) 0, th = guitab_head; th; th = th->next) {
	if (strcmp(th->name, gui) == 0) {
	    found = th;
	    break;
	}
    }
    if (!found) {
	return (0);
    }

    for (t = trigger_head[swap_trg_ptr].next_trigger,
	 t_prev = &trigger_head[swap_trg_ptr];
	 (t != (TRIGGER_PTR) 0) && (t->time <= time);
	 t_prev = t, t = t->next_trigger) {
	if ((t->time == time) &&
	    (strcmp(t->guitab_ptr->name, gui) == 0) &&
	    (strcmp(t->name, name) == 0) && (strcmp(t->cmd, cmd) == 0)
	    ) {
	    /*
	     * Experiment-Zuordnung und Aktiv-Flag koennen geaendert sein! 
	     */
	    t->aflag = aflag;
	    t->hidden = hidden;
	    t->experiment_mask = exp_mask;

	    return (0);
	}
    }

    if ((trg = alloc_trigger()) == (TRIGGER_PTR) 0) {
	return (0);
    }

    trg->next_trigger = t;
    trg->prev_trigger = t_prev;
    t_prev->next_trigger = trg;
    if (t)
	t->prev_trigger = trg;

    trg->time = time;
    trg->aflag = aflag;
    trg->hidden = hidden;
    trg->experiment_mask = exp_mask;
    trg->guitab_ptr = found;
    sprintf(trg->name, "%s", name);
    sprintf(trg->cmd, "%s", cmd);

    if (debug > 6) {
	syslog(LOG_INFO, "set_trigger: %s %d %s %s \n",
	       trg->name, trg->time, trg->guitab_ptr->name, trg->cmd);
    }

    return (1);
}

int
delete_trigger(number)
     int             number;
{
    TRIGGER_PTR     trg,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found;

    for (found = 0, i = 1,
	 t = trigger_head[swap_trg_ptr].next_trigger,
	 t_prev = &trigger_head[swap_trg_ptr];
	 t != (TRIGGER_PTR) 0; t_prev = t, t = t->next_trigger, i++) {
	if (i == number) {
	    found = i;
	    break;
	}
    }

    if (!found) {
	return (0);
    }

    t_prev->next_trigger = t->next_trigger;
    if (t->next_trigger)
	t->next_trigger->prev_trigger = t_prev;

    free_trigger(t);
    return (1);
}

int
rm_trigger(name, time, gui, cmd)
     char           *name;
     unsigned int    time;
     char           *gui;
     char           *cmd;
{
    TRIGGER_PTR     trg,
                    t,
                    t_prev,
                    t_next;
    int             i,
                    found = 0;

    if (!trg_tab_copied)
	cp_trigger_table();

    for (i = 1, t = trigger_head[swap_trg_ptr].next_trigger;
	 t != (TRIGGER_PTR) 0; t = t->next_trigger, i++) {
	if ((t->time == time) &&
	    (strcmp(t->guitab_ptr->name, gui) == 0) &&
	    (strcmp(t->name, name) == 0) && (strcmp(t->cmd, cmd) == 0)
	    ) {
	    if (debug > 6) {
		syslog(LOG_INFO, "rm_trigger: %s %d %s DELETE \n",
		       name, time, cmd);
	    }
	    found = 1;
	    delete_trigger(i);
	}
    }
    return (found);
}

void
clock_tick(sig)
     int             sig;
{
    int             rtn;
    int             t;
    GUITAB         *gui_num;
    char            str[512];
    int             i,
                    k,
                    l;
    GUITAB         *tmp_gui;
    int             exp_ok;
    GUITAB         *th;

    lost_ticks++;

    /*
     * Damit kein Tick in die Wueste geht 
     */
    sigact.sa_handler = clock_tick;
    sigaction(SIGALRM, &sigact, NULL);
/***********
      signal (SIGALRM, clock_tick);
*********/

    t = lost_ticks;
    lost_ticks -= t;

    if (!timer_stopped) {
	if ((last_exp_index != exp_index) ||
	    (last_cycle_number != cycle_number)) {
	    if (swap_trigger_table) {
		/*
		 * hidden trigger uebernehmen!!! 
		 */
		int             swap = curr_trg_ptr;
		HIDDEN_LIST    *hl;
		for (hl = hidden_trigger_head; hl; hl = hl->next) {
		    rtn =
			set_trigger(hl->name, hl->time, hl->gui, hl->aflag,
				    hl->cmd, hl->experiment_mask,
				    hl->hidden);
		}
		curr_trg_ptr = swap_trg_ptr;
		swap_trg_ptr = swap;
		current_trigger_head = &trigger_head[curr_trg_ptr];
		swap_trigger_table = 0;
		trg_tab_copied = 0;

	    }
	    next_trigger = current_trigger_head->next_trigger;
	    last_cycle_number = cycle_number;
	    last_exp_index = exp_index;
	}

	if (status == 3) {
	    intern_cycle_time += (interval * t);
	    intern_cycle_time %= cycle_duration;
	} else {
	    last_cycle_number = 0;
	}

	while (next_trigger && (next_trigger->time <= intern_cycle_time)) {
	    gui_num = next_trigger->guitab_ptr;

	    /*
	     * hidden trigger muessen immer gesendet werden unabhaengig
	     * vom Zustand des Servers Format :
	     * 
	     * ">(Oberflaeche,sw_timing) sw_trigger Methode Zeit Dauer
	     * Experiment"
	     * 
	     * z.B. ">(tx,sw_timing) sw_trigger Macro_intern_Trigger 1000
	     * 20000 1" 
	     */

	    exp_ok = ((1 << curs) & next_trigger->experiment_mask) &&
		(go || next_trigger->hidden);

	    if (gui_num && gui_num->active_socket && next_trigger->aflag
		&& exp_ok) {
		str[0] = '\0';
		sprintf(str, ">(%s,%s) %s %d %d %d %d\n",
			gui_num->name, "sw_timing", next_trigger->cmd,
			intern_cycle_time, cycle_duration,
			curs, cycle_number);

		if (debug > 6) {
		    syslog(LOG_INFO, "%s", str);
		}

		if (write(gui_num->active_socket, str, strlen(str)) == -1) {
		    perror("Fehler 1: ");
		    if (errno != EINTR) {
			GUITAB         *s_swap = this_gui;
			this_gui = gui_num;
			release_connection();
			this_gui = s_swap;
		    }
		}
	    }
	    next_trigger = next_trigger->next_trigger;

	}
    }

    is_et_su_widd += (t * interval);
    if (is_et_su_widd >= alarm_intvl * 1000) {	/* alle 75 000 ms */
	is_et_su_widd = 0;
	for (th = guitab_head; th; th = th->next) {

	    /*
	     * Wenn Verbindung besteht, pruefen!!!!! 
	     */
	    if (th->active_socket != 0) {
		/*
		 * Verbindung unterbrochen => loesen!!! 
		 */
		if (th->not_alive > (10 * alarm_intvl)) {
		    tmp_gui = this_gui;
		    this_gui = th;
		    release_connection();
		    this_gui = tmp_gui;
		}
/**
      Immer noch keine Antwort: weiter pruefen!
**/
		else if (th->not_alive > 0) {
		    th->not_alive += alarm_intvl;

		    l = write(th->active_socket, ping, strlen(ping));
		    if ((l == -1) && (errno != EINTR)) {
			tmp_gui = this_gui;
			this_gui = th;
			release_connection();
			this_gui = tmp_gui;
		    }
		}
/***
     Wenn zwischenzeitlich kein Lebenszeichen, wird beim naechsten Mal geprueft
******/
		else if (th->not_alive == 0) {
		    th->not_alive++;
		}
	    }
	}
    }
}

void
timer_set_up()
{
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = usec;	/* 100 ms */

    stp_timer.it_value.tv_sec = 1;
    stp_timer.it_value.tv_usec = 0;
    stp_timer.it_interval.tv_sec = 1;	/* 1 s */
    stp_timer.it_interval.tv_usec = 0;
}

void
stop_timer()
{
    sigact.sa_handler = clock_tick;
    sigaction(SIGALRM, &sigact, NULL);
/***********
   signal (SIGALRM, clock_tick);
*********/
    setitimer(ITIMER_REAL, &stp_timer, 0);
    last_exp_index = 0;
    last_cycle_number = -1;
    intern_cycle_time = 0;
    interval = stp_timer.it_interval.tv_sec * 1000;	/* 1 000 ms */
    timer_stopped = 1;
}

void
start_timer()
{
    sigact.sa_handler = clock_tick;
    sigaction(SIGALRM, &sigact, NULL);
/***********
   signal (SIGALRM, clock_tick);
*********/
    setitimer(ITIMER_REAL, &timer, 0);
    interval = USECONDS / 1000;	/* 100 ms */
    timer_stopped = 0;
}

int
disconn(gt)
     GUITAB         *gt;
{
    int             socket = gt->reception_socket;
    if (debug) {
	syslog(LOG_INFO, "CLOSE: %s port: %d \n",
	       gt->name, gt->sw_timing_port);
    }

    close(socket);
    gt->reception_socket = 0;
    FD_CLR(socket, &readmask);
    tcpmask[socket] = 0;
}

void
release_connection()
{
    char           *tm_str;
    time_t          t;
    int             tmp_socket;

    int             socket = this_gui->active_socket;
    if ((socket) && (this_gui != (GUITAB *) 0)) {
	FD_CLR(socket, &readmask);
	tcpmask[socket] = 0;
	close(socket);

	t = time(0);
	tm_str = (char *) ctime(&t);
	tm_str[strlen(tm_str) - 1] = '\0';
	{
	    syslog(LOG_NOTICE,
		   "%s: Socket: %d Oberflaeche: \"%s\" geschlossen \n",
		   tm_str, this_gui->active_socket, this_gui->title);
	}

	if (socket == tx_gui_socket) {
	    tx_gui_socket = 0;
	}
	this_gui->inet_addr = 0;
	this_gui->active_socket = 0;
	information();
    }
}

/*
 * Wird bei Empfang vom VME-Timing Sender aufgerufen 
 */
void
inform_clients(char *list)
{
    char            s[512];
    int             i_status,
                    i_modus,
                    i_cycle_duration,
                    i_cycle_number,
                    i_cycle_time,
                    i_exp_wechsel,
                    i_exp_index;
    int             check = 0;
    int             dest_id,
                    src_id;

    check = sscanf(list, "()%s :%[^:]:%d:%d:%d:%d:%d:%d:%d",
		   gui_name, gui_cmd, &i_status, &i_modus,
		   &i_cycle_duration, &i_cycle_number, &i_cycle_time,
		   &i_exp_wechsel, &i_exp_index);
    if (check != 9) {
	check = sscanf(list,
		       "(%d,%d)%s :%[^:]:%d:%d:%d:%d:%d:%d:%d",
		       &dest_id, &src_id, gui_name, gui_cmd, &i_status,
		       &i_modus, &i_cycle_duration,
		       &i_cycle_number, &i_cycle_time, &i_exp_wechsel,
		       &i_exp_index);
    }

    /*
     * Timing Sender Oberflaeche muss diese Information auf jeden Fall
     * erhalten 
     */

    if (tx_udp_send_socket) {
	if (sendto(tx_udp_send_socket, list, strlen(list), 0,
		   (struct sockaddr *) &tx_usta_addr_out, addrlen) == -1) {
	    if (debug > 6) {
		syslog(LOG_INFO, "Error: \"->tx: \"%s\"", list);
	    }
	}
    }

/*** Sonst stimmt etwas mit dem Format nicht! */
    if ((check != 9) && (check != 11))
	return;

    curm = (i_status & 0xff0000) >> 16;
    curs = (i_status & 0x00ff00) >> 8;
    status = i_status & 0xff;
    eventcount = i_modus >> 8;
    modus = i_modus & 0xff;
    cycle_duration = i_cycle_duration;
    /*
     * last_cycle_number = cycle_number; 
     */
    cycle_number = i_cycle_number;
    cycle_time = i_cycle_time;
    exp_wechsel = i_exp_wechsel;
    last_exp_index = exp_index;
    exp_index = i_exp_index + 1;
    intern_cycle_time = cycle_time;
    lost_ticks = 0;
    if (old_status != status) {
	switch (status) {
	case 3:
	case 5:
	    start_timer();
	    break;
	default:
	    stop_timer();
	    break;
	}
/**** POCO_START ******************/
	if (tx_gui_socket) {
	    current_exp = 1;
	    sprintf(s, ">(tx,myself) timstx return_poco_start %d\n",
		    current_exp);
	    write(tx_gui_socket, s, strlen(s));
	}
	old_status = status;
    }


    /*
     * Weitere Clienten??? 
     */
    if (inform_head.next_inform) {
	INFORM_PTR      t;
	int             l;


/******
 Format:
 :usta:$x6:$x2:$x3:$i_cyc_count($p_num):$i_cyc_time($p_num):$exp_wechsel:$p_num:$poco_start($p_num):"
********/

	sprintf(s, "()%s :%s:%d:%d:%d:%d:%d:%d:%d:%d",
		gui_name, gui_cmd, status, modus, cycle_duration,
		cycle_number, cycle_time, exp_wechsel, curs,
		poco_start[curs]);
	l = strlen(s);
	for (t = inform_head.next_inform;
	     t != (INFORM_PTR) 0; t = t->next_inform) {
	    sendto(t->socket, s, l, 0, (struct sockaddr *) &t->send_addr,
		   addrlen);
	}
    }
}

void
information()
{
    char            tmp[256];
    char            list[512];
    struct in_addr  i_addr;

    if (do_inform) {
	list[0] = '\0';
	strcat(list, this_gui->name);
	strcat(list, ":");
	i_addr.s_addr = htonl(this_gui->inet_addr);
	strcat(list, inet_ntoa(i_addr));
	strcat(list, ":");
	sprintf(tmp, "%d", this_gui->sw_timing_port);
	strcat(list, tmp);
	strcat(list, "\n");


	if (write(do_inform, list, strlen(list)) == -1) {
	    if (errno != EINTR) {
		GUITAB         *s_swap = this_gui;
		this_gui = this_inform;
		release_connection();
		do_inform = 0;
		this_gui = s_swap;
	    }
	}
    }
}


void
check_all(argc, argv)
     int             argc;
     char           *argv[];
{
    char           *tmp;
    int             i,
                    j,
                    k;
    int             c;
    int             failure;

    if (debug < 3) {
	if (fork() != 0)
	    exit(0);
    }
/*********
********/

    msgs = (struct cctpmsg *) malloc(sizeof(struct cctpmsg));
    addrlen = client_addrlen = sizeof(struct sockaddr_in);
    recv_addrlen = sizeof(struct sockaddr_in);
    memset((void *) &recv_addr, 0, (size_t) recv_addrlen);
    memset((void *) &client_addr, 0, (size_t) addrlen);
    memset((void *) &tx_usta_addr_in, 0, sizeof(struct sockaddr_in));
    memset((void *) &tx_usta_addr_out, 0, sizeof(struct sockaddr_in));

    failure = gethostname(host_name, 257);
    if (failure) {
	fprintf(stderr, "Error with gethostname, exiting\n");
	exit(1);
    }
    host_entry = gethostbyname(host_name);
/*******
   my_inet_addr = ntohl(*(unsigned long*) host_entry->h_addr);
*********/
    my_inet_addr = INADDR_ANY;

    if ((tmp = getenv("TCL")) == (char *) 0) {
	sprintf(gui_conf_name, "%s/%s", default_conf_dir,
		default_conf_name);
	sprintf(hidden_trigger_list, "%s/%s", default_conf_dir,
		default_hidden_trigger_list);
	sprintf(hidden_inform_list, "%s/%s", default_conf_dir,
		default_hidden_inform_list);
	sprintf(gui_host_file, "%s/hosts", default_conf_dir);
	sprintf(sw_timing_pid_name, "%s/%s.%s",
		default_pid_dir, default_pid_name, host_name);
    } else {
	sprintf(gui_conf_name, "%s/conf/%s", tmp, default_conf_name);
	sprintf(gui_host_file, "%s/conf/hosts", tmp);
	sprintf(sw_timing_pid_name, "%s/etc/%s.%s",
		tmp, default_pid_name, host_name);
	sprintf(hidden_trigger_list, "%s/conf/%s", tmp,
		default_hidden_trigger_list);
	sprintf(hidden_inform_list, "%s/conf/%s", tmp,
		default_hidden_inform_list);
    }

    sprintf(sw_timing_host_name, "unknown");
    gui_host = fopen(gui_host_file, "r");
    if (gui_host == 0) {
	syslog(LOG_INFO, "Datei %s konnte nicht geoeffnet werden",
	       gui_host_file);
	exit(1);
    }
    while ((tmp = fgets(line, LINE_LEN, gui_host)) != (char *) 0) {
	if ((i = sscanf(line, "%s %s", program_name, sw_timing_host_name))
	    == 2) {
	    if (strcmp(program_name, argv[0]) == 0)
		break;
	}
    }
    if (strcmp( sw_timing_host_name, "localhost") != 0 && strcmp(host_name, sw_timing_host_name) != 0) {
	syslog(LOG_INFO, "%s muss auf \"%s\" gestartet werden",
	       argv[0], sw_timing_host_name);
	exit(2);
    }


return;

/**** Ein anderer Server laeuft schon!!!!! ***********/
    if ((sw_timing_pid_file = fopen(sw_timing_pid_name, "r")) != 0) {
	fscanf(sw_timing_pid_file, "%d", &sw_timing_pid);
	fclose(sw_timing_pid_file);
	errno = 0;
	if (kill(sw_timing_pid, SIGUSR1) != 0) {
/***** Der Server ist abgestuerzt und "sw_timing_pid" uebriggeblieben ****/
	    if (errno == ESRCH)
		unlink(sw_timing_pid_name);
	    else
		exit(1);
	} else
	    exit(0);
    }


    sw_timing_pid_file = fopen(sw_timing_pid_name, "w+");
    if (sw_timing_pid_file == 0) {
	syslog(LOG_INFO, "%s konnte nicht geoeffnet werden!",
	       sw_timing_pid_name);
	exit(2);
    }
    sw_timing_pid = getpid();
    i = fprintf(sw_timing_pid_file, "%d", sw_timing_pid);
    fflush(sw_timing_pid_file);
    fclose(sw_timing_pid_file);
}

void
table_init()
{
    int             i,
                    j,
                    k;
    GUITAB         *ix;

    FD_ZERO(&readmask);

    for (k = 0; k < MAX_SOCKETS; k++) {
	guitab[k].inet_addr = ntohl(0);
	guitab[k].reception_socket = 0;
	guitab[k].active_socket = 0;
	guitab[k].next = &guitab[k + 1];
	guitab[k + 1].prev = &guitab[k];
	tcpmask[k] = 0;
    }
    guitab[MAX_SOCKETS - 1].next = (GUITAB *) 0;
    guitab[0].prev = (GUITAB *) 0;
    guitab_head = (GUITAB *) 0;
    free_guitab_head = &guitab[0];
}

main(argc, argv)
     int             argc;
     char           *argv[];
{
    char           *tmp;
    int             i,
                    j,
                    k;
    TRIGGER_PTR     t;
    int             c;
    char           *log_file_name = (char *) 0;
    extern char    *optarg;

    /*
     * Option "-l" : Logfile 
     */
    log_file_fd = stderr;
    openlog("sw_timing", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

    while ((c = getopt(argc, argv, "d:l:")) != -1) {
	switch (c) {
	case 'd':
	    debug = atoi(optarg);
	    break;
	case 'l':
	    log_file_name = optarg;
	    break;
	default:
	    break;
	}
    }
    if (log_file_name != (char *) 0) {
	if ((log_file_fd = fopen(log_file_name, "w+")) == (FILE *) 0)
	    log_file_fd = stderr;
    }

    sigact.sa_handler = sig_handling;
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGABRT, &sigact, NULL);
    if (debug < 3) {
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
    }
    sigact.sa_handler = configure;
    sigaction(SIGUSR1, &sigact, NULL);

    check_all(argc, argv);
    table_init();
    configure(SIGUSR1);
    init_trigger();
    init_inform();

    timer_set_up();
    stop_timer();

    sw_timing();
}

sw_timing()
{
    char            name[257],
                    title[64],
                    host[64],
                    display[64];
    int             id,
                    i,
                    j,
                    k;		/* Zaehlindex */
    GUITAB         *ix;
    int             imsg,
                    icount,
                    irec;	/* empfangene Zeichen */
    int             c;
    char            dest[80],
                    src[80],
                    cmd[128];
    int             check;
    char           *tm_str;
    time_t          t;
    struct in_addr  i_addr;
    char            strg[256];


    /*
     * Empfangsschleife 
     */
    while (1) {
	FD_ZERO(&readfds);
	FD_ZERO(&excepfds);
	for (k = 0; k < (MAX_SOCKETS); k++) {
	    if (FD_ISSET(k, &readmask)) {
		FD_SET(k, &readfds);
		FD_SET(k, &excepfds);
	    }
	}

/****

 *    Der Timer bricht den select-SystemCall ab, ohne dass readfds oder excepfds
 *    veraendert wurden. Daher muss bei dem Fehler EINTR (Interrupted system call)
 *    select wieder aufgenommen werden!!!

      while  ((nfound = select (MAX_SOCKETS, &readfds, 0, &excepfds, NULL))== -1) 
****/
	if ((nfound =
	     select(MAX_SOCKETS, &readfds, 0, &excepfds, 0)) == -1) {
	    switch (errno) {
	    case EINTR:
		if (debug > 12)
		    perror("select Error ==> ");
		break;

	    case EBADF:
		if (debug > 4)
		    perror("select Error ==> ");
		break;

	    default:
		break;
	    }
	    continue;
	}

	for (j = 0, this_socket = 0, k = 0; (k < (MAX_SOCKETS)) && nfound;
	     j++, this_socket++, k++) {
	    if (tcpmask[this_socket] & FD_ISSET(k, &excepfds)) {

		if (debug > 6) {
		    syslog(LOG_INFO, "EXCEPTION: %d errno: %d",
			   this_socket, errno);
		}

		this_gui = (GUITAB *) 0;
		for (ix = guitab_head; ix; ix = ix->next) {
		    if (this_socket == ix->active_socket) {
			this_gui = ix;
			break;
		    }
		}
		release_connection();
	    }
	}

	for (j = 0, this_socket = 0, k = 0; (k < (MAX_SOCKETS)) && nfound;
	     j++, this_socket++, k++) {
	    if (tcpmask[this_socket] && FD_ISSET(k, &readfds)) {
		this_gui = (GUITAB *) 0;
		for (ix = guitab_head; ix; ix = ix->next) {
		    if ((this_socket == ix->active_socket) ||
			(this_socket == ix->reception_socket)) {
			this_gui = ix;
			break;
		    }
		}
		if (debug > 10) {
		    syslog(LOG_INFO,
			   "recvfrom (this_socket, msgs->mtext, ...");
		}
		irec = recvfrom(this_socket,
				msgs->mtext, MAXMSG, 0,
				(struct sockaddr *) &recv_addr,
				&recv_addrlen);
		nfound--;
		if ((irec == -1) && (errno == ENOTCONN)) {
		    if (debug > 6) {
			syslog(LOG_INFO,
			       "<== recvfrom: %d irec ERROR %d", irec,
			       errno);
		    }
/*** toc toc toc!!! is there anybody? ************************************/
		    recv_addrlen = sizeof(struct sockaddr_in);
		    s = accept(this_gui->reception_socket,
			       (struct sockaddr *) &recv_addr,
			       &recv_addrlen);

		    if (s != -1) {
/**** come in! Oberflaeche noch nicht angemeldet ******************/
			if (this_gui->active_socket == 0) {
			    this_gui->inet_addr =
				ntohl(recv_addr.sin_addr.s_addr);
			    this_gui->not_alive = 0;
			    this_gui->active_socket = s;
			    t = time(0);
			    tm_str = (char *) ctime(&t);
			    tm_str[strlen(tm_str) - 1] = '\0';
	      /***   if (debug) ***/
			    i_addr.s_addr = htonl(this_gui->inet_addr);
			    host_entry = gethostbyaddr((char *)
						       &(i_addr.s_addr),
						       sizeof(struct
							      in_addr),
						       AF_INET);

			    if (host_entry == NULL)
				host_entry = &gethostbyXXX;
			    {
				syslog(LOG_NOTICE,
				       "%s: %s %d Oberflaeche: \"%s\" von %s geoeffnet",
				       tm_str, "Socket:",
				       this_gui->active_socket,
				       this_gui->title,
				       host_entry->h_name);
			    }

			    tcpmask[s] = 1;
			    FD_SET(s, &readmask);

			    prompt("$\n");
			}
/**** stay out! Oberflaeche schon angemeldet ******************/
			else {
			    i_addr.s_addr = htonl(this_gui->inet_addr);
			    host_entry = gethostbyaddr((char *)
						       &(i_addr.s_addr),
						       sizeof(struct
							      in_addr),
						       AF_INET);

			    if (host_entry == NULL)
				host_entry = &gethostbyXXX;


			    sprintf(mtext, "! %s %s \n",
				    host_entry->h_name, inet_ntoa(i_addr));
			    write(s, mtext, strlen(mtext));
			    if (debug > 3)
				syslog(LOG_INFO, "In Use ======>  %s",
				       mtext);
			    close(s);
			}
		    } else {
			syslog(LOG_WARNING,
			       "GUI: %s error in accept: %d",
			       this_gui->name, errno);
		    }

		}
/************ good bye! Oberflaeche beendet ***********/
		if (irec == 0) {
		    release_connection();
		    continue;
		}
/******** ??? ***************************************/
		else if (msgs->mtext[0] == '\0')
		    continue;

		msgs->mtext[irec] = '\0';

		if (this_gui)
		    this_gui->not_alive = 0;

		if (debug > 6) {
		    syslog(LOG_INFO, "socket: %d \"%s\"",
			   this_socket, msgs->mtext);
		}
		if (this_socket == tx_udp_recv_socket) {
		    if (debug > 6) {
			syslog(LOG_INFO, "---> inform_clients");
		    }
		    inform_clients(msgs->mtext);
		    if (debug > 6) {
			syslog(LOG_INFO, "<--- inform_clients");
		    }
		    continue;
		}

		if ((msgs->mtext[irec - 1] != '\n') &&
		    (msgs->mtext[irec - 1] != '\r')) {
		    msgs->mtext[irec++] = '\n';
		}
		msgs->mtext[irec] = '\0';

		imsg = icount = 0;
		while (icount = sgetline(&(msgs->mtext)[imsg], mtext)) {
		    imsg += icount;
		    mtext[icount] = '\0';
/******** ??? ***************************************/
		    switch (mtext[0]) {
		    case '$':
			/*
			 * Befehl an den Server!
			 */
			switch (mtext[1]) {
			case 'G':	/* $G = go */
			    go = 1;
			    sprintf(gui_list, "G\n");
			    if (write(this_gui->active_socket,
				      gui_list, strlen(gui_list)) == -1)
				if (errno != EINTR)
				    release_connection();
			    break;
			case 'S':	/* $S = stop */
			    go = 0;
			    sprintf(gui_list, "S\n");
			    if (write(this_gui->active_socket,
				      gui_list, strlen(gui_list)) == -1)
				if (errno != EINTR)
				    release_connection();
			    break;
			case 'g':	/* "gc" */
			    switch (mtext[2]) {
				/*
				 * Liste aller Oberflaechen mit Title
				 */
				/*
				 * $gS Status G = running S = stopped 
				 */
			    case 'S':
				if (go)
				    sprintf(gui_list, "G\n");
				else
				    sprintf(gui_list, "S\n");
				if (write(this_gui->active_socket,
					  gui_list,
					  strlen(gui_list)) == -1)
				    if (errno != EINTR)
					release_connection();
				break;

			    case 't':
				gui_list[0] = '\0';
				for (ix = guitab_head; ix; ix = ix->next) {
				    char            tmp[40];

				    sprintf(tmp, " {%s:%s}",
					    ix->name, ix->title);
				    strcat(gui_list, tmp);
				}
				strcat(gui_list, "\n");
				if (write(this_gui->active_socket,
					  gui_list,
					  strlen(gui_list)) == -1)
				    if (errno != EINTR)
					release_connection();
				break;
				/*
				 * Liste aller aktiven Oberflaechen mit Title
				 */
			    case 'a':
				gui_list[0] = '\0';
				for (ix = guitab_head; ix; ix = ix->next) {
				    char            tmp[40];

				    if (ix->active_socket) {
					sprintf(tmp, " {%s:%s}",
						ix->name, ix->title);
					strcat(gui_list, tmp);
				    }
				}
				strcat(gui_list, "\n");
				if (write(this_gui->active_socket,
					  gui_list,
					  strlen(gui_list)) == -1)
				    if (errno != EINTR)
					release_connection();
				break;
			    default:
				if (debug > 3) {
				    syslog(LOG_INFO,
					   "%s: No such command \"%s\"",
					   "server", mtext);
				}
				break;
			    }
			    break;
/*** Timeout setzen und loeschen! ******************/
			case 't':	/* "gc" */
			    switch (mtext[2]) {
				char            xn[32],
				                xc[256],
				                xg[80];
				unsigned long   xe;
				int             xt,
				                xf;
				/*
				 * setzen : $ts Trigger1 swtest 1000 manage trigger1 abc
				 * Format: "Name" "gui" time flag "command" experiment_mask
				 */
			    case 's':
				if ((check = sscanf(&mtext[4],
						    set_format,
						    xn, xg, &xt, &xf, xc,
						    &xe)) >= 6) {
				    set_trigger(xn, xt, xg, xf, xc, xe, 0);
				}
				prompt("$\n");
				break;
				/*
				 * Trigger loeschen : $td Trigger1 swtest 1000 manage trigger1 abc
				 */
			    case 'd':
				if ((check = sscanf(&mtext[4],
						    "\"%[^\"] %*s \"%[^\"] %*s %d %d \"%[^\"]",
						    xn, xg, &xt, &xf,
						    xc)) >= 5) {
				    if (rm_trigger(xn, xt, xg, xc))
					prompt("$\n");
				    else
					prompt("error\n");
				} else
				    prompt("errorn");
				break;
				/*
				 * Tabelle loeschen : $tc Trigger1 swtest 1000 manage trigger1 abc
				 */
			    case 'c':
				clear_trigger_tab();
				prompt("$\n");
				break;
				/*
				 * Tabelle listen : $tl 
				 */
			    case 'l':
				if (!list_trigger_tab())
				    prompt("$\n");
				break;

				/*
				 * Tabelle swappen : $tw 
				 */
			    case 'w':
				swap_trigger_table = 1;
				prompt("$\n");
				break;

			    default:
				if (debug > 3) {
				    syslog(LOG_INFO,
					   "%s: No such command \"%s\"\n",
					   "sw_timing", mtext);
				}
				break;
			    }
			    break;

			case 'u':	/* "gc" */
			    switch (mtext[2]) {
				char            xh[80];
				int             xp;
				/*
				 * setzen : $us port hostname
				 */
			    case 's':
				if ((check = sscanf(&mtext[4],
						    "%d %s", &xp,
						    xh)) >= 2) {
				    set_inform(xp, xh);
				}
				break;
				/*
				 * loeschen : $ud port hostname
				 */
			    case 'd':
				if ((check = sscanf(&mtext[4],
						    "%d %s", &xp,
						    xh)) >= 2) {
				    rm_inform(xp, xh);
				}
				break;
				/*
				 * listen : $ul 
				 */
			    case 'l':
				list_inform(xp, xh);
				break;

			    default:
				if (debug > 3) {
				    syslog(LOG_INFO,
					   "%s: No such command \"%s\"",
					   "sw_timing", mtext);
				}
				break;
			    }
			    break;
/**** Information bei Veraenderungen! ******/
			case 'i':
			    if (tx_gui_socket == 0) {
	/******** UDP Port zum Timing Sender Target oeffnen *********/
				sscanf(&mtext[3], "%d %d",
				       &tx_udp_recv_port,
				       &tx_udp_send_port);
				if (tx_udp_recv_socket) {
				    FD_CLR(tx_udp_recv_socket, &readmask);
				    close(tx_udp_recv_socket);
				    tx_udp_recv_socket = 0;
				}
				tx_udp_recv_socket =
				    create_recv_socket(htons
						       (tx_udp_recv_port),
						       &tx_usta_addr_in);
				if (debug > 3) {
				    syslog(LOG_NOTICE,
					   "Listening to UDP-Socket %d Port: %dn",
					   tx_udp_recv_socket,
					   tx_udp_recv_port);
				}
				tx_gui_socket = this_socket;
				tcpmask[tx_udp_recv_socket] = 1;
				FD_SET(tx_udp_recv_socket, &readmask);

				tx_usta_addr_out.sin_addr.s_addr =
				    htonl(this_gui->inet_addr);
				tx_usta_addr_out.sin_family = AF_INET;
				tx_usta_addr_out.sin_port =
				    htons(tx_udp_send_port);
			    }
			    break;

/**** Information beenden! ******/
			case 'c':
			    do_inform = 0;
			    tx_udp_recv_socket = 0;
			    break;

			default:
			    if (debug > 3) {
				syslog(LOG_INFO,
				       "%s: No such command \"%s\"",
				       "sw_timing", mtext);
			    }
			    break;
			}	/* switch (mtext[1]) */
			break;

/**** Nachrichtenaustausch zwischen Oberflaechen ******/
		    case '>':
		    case '<':
			if ((check = sscanf(&mtext[1],
					    "(%[^ ,] %*c %[^ )]) %[\n-\176]",
					    dest, src, cmd)) == 3) {
			    GUITAB         *found;

			    if ((mtext[0] == '<')
				&& (!strcmp(dest, "myself"))) {
				sscanf(cmd, "%d",
				       &poco_start[current_exp]);
				if (current_exp != 0) {
				    current_exp++;
				    current_exp %= (MAX_EXP + 1);
				    if (current_exp && tx_gui_socket) {
					sprintf(strg,
						">(tx,myself) timstx return_poco_start %d\n",
						current_exp);
					write(tx_gui_socket, strg,
					      strlen(strg));
				    }
				}
			    }

			    else {
				for (found = (GUITAB *) 0, ix =
				     guitab_head; ix; ix = ix->next) {
				    if (strcmp(ix->name, dest) == 0) {
					found = ix;
					break;
				    }
				}
				if ((found) && (ix->active_socket)) {
				    if (write(ix->active_socket,
					      mtext, strlen(mtext)) == -1)
				    {
					if (errno != EINTR) {
					    GUITAB         *s_swap =
						this_gui;
					    this_gui = ix;
					    release_connection();
					    this_gui = s_swap;
					}
				    }
				} else
				    if (write(this_gui->active_socket, "\n", 1) == -1)
				    if (errno != EINTR)
					release_connection();
			    }
			}
			break;

		    case 'P':
			if (strncmp(mtext, "PING", 4) == 0) {
			    this_gui->not_alive = 0;

			    if (debug > 6) {
				syslog(LOG_INFO, "%s  not_alive: %d",
				       this_gui->name,
				       this_gui->not_alive);
			    }
			}
			break;


		    }		/* switch ( mtext[0]) */

		}
	    }
	}

    }				/* while (1) */

}

/*
 * Special Reaction on received Signal ... (Ignorance or Termination)
 */

void
sig_handling(sig)
     int             sig;
{
    int             i,
                    j;
    GUITAB         *ix;

    switch (sig) {
	/*
	 * Der Server darf nicht beendet werden!!!!! 
	 */
    case SIGINT:
    case SIGTERM:
    case SIGPIPE:
	sigact.sa_handler = sig_handling;
	sigaction(sig, &sigact, NULL);
	break;
    default:
	syslog(LOG_WARNING, "Termination (%d) on signal %d.",
	       getpid(), sig);
	for (ix = guitab_head; ix; ix = ix->next) {
	    if (ix->active_socket)
		close(ix->active_socket);
	    if (ix->reception_socket)
		close(ix->reception_socket);
	}

	unlink(sw_timing_pid_name);
	closelog();
	exit(1);
	break;
    }
}

GUITAB         *
allocate_guitab()
{
    GUITAB         *tmp;
    GUITAB         *gp = free_guitab_head;
    if (free_guitab_head != (GUITAB *) 0) {
	free_guitab_head = gp->next;
	if (free_guitab_head)
	    free_guitab_head->prev = (GUITAB *) 0;

	if (guitab_head)
	    guitab_head->prev = gp;
	gp->next = guitab_head;
	gp->prev = (GUITAB *) 0;
	guitab_head = gp;
    }
    return (gp);
}

void
free_guitab(gp)
     GUITAB         *gp;
{
    GUITAB         *tmp;
    tmp = gp->next;
    if (tmp)
	tmp->prev = gp->prev;
    tmp = gp->prev;
    if (tmp)
	tmp->next = gp->next;
    else
	guitab_head = gp->next;

    gp->next = free_guitab_head;
    gp->prev = (GUITAB *) 0;
    free_guitab_head = gp;
}

void
configure(sig)
     int             sig;
{
    char           *tmp;
    int             i,
                    k,
                    l;
    GUITAB         *tmp_gui;
    GUITAB         *gt,
                   *th;
    char            name[128];
    char            gui[128];
    char            cmd[128];
    unsigned int    time;
    HIDDEN_LIST    *hl;

    for (th = guitab_head; th; th = th->next) {
	th->is_listed = 0;
    }

    gui_conf = fopen(gui_conf_name, "r");
    if (gui_conf == 0) {
	fprintf(log_file_fd, "Datei %s konnte nicht geoeffnet werden\n",
		gui_conf_name);
	exit(1);
    }

    while ((tmp = fgets(line, LINE_LEN, gui_conf)) != (char *) 0) {
	gt = allocate_guitab();
	i = read_tab(line, gt);
	if (i >= 4) {
	    for (th = gt->next; th; th = th->next) {
		if ((gt->sw_timing_port == th->sw_timing_port) ||
		    (strcmp(gt->name, th->name) == 0)) {
		    th->is_listed = 1;
		    break;
		}
	    }
	    if (th != (GUITAB *) 0)
		free_guitab(gt);
	    else {
		gt->reception_socket = 0;
		gt->active_socket = 0;
		gt->is_listed = 1;
/*******
fprintf (log_file_fd, "Neu: %s Port %d auf %s geoeffnet  \n", 
                  gt->name, gt->sw_timing_port, host_name);
**********/
	    }
	} else
	    free_guitab(gt);
    }
    fclose(gui_conf);

/********************************************************************/
    for (th = guitab_head; th; th = th->next) {
	if ((th->is_listed == 0) && (th->active_socket == 0)) {
/********
fprintf (log_file_fd, "Weg: %s Port %d auf %s geoeffnet  \n", 
                  th->name, th->sw_timing_port, host_name);
*********/
	    disconn(th);
	    gt = th->prev;
	    free_guitab(th);
	    if (!gt)
		gt = guitab_head;
	    th = gt;
	}
    }
/********************************************************************/

    for (th = guitab_head; th; th = th->next) {
	if (th->reception_socket == 0) {
	    if ((s =
		 tpopen(htons(th->sw_timing_port), my_inet_addr)) == -1) {
		perror("Fehler beim Oeffnen: ");
		continue;
	    }

	    fcntl(s, F_SETFL, O_NONBLOCK);

	    syslog(LOG_NOTICE,
		   "GUI: %s Socket: %d Port %d auf %s geoeffnet",
		   th->name, s, th->sw_timing_port, host_name);

	    th->reception_socket = s;
	    th->active_socket = 0;
	    tcpmask[s] = 1;
	    FD_SET(s, &readmask);
	}
    }

    /*
     * Versteckte Trigger loeschen
     */
    for (hl = hidden_trigger_head; hl; hl = hl->next) {
	k = rm_trigger(hl->name, hl->time, hl->gui, hl->cmd);
    }

    /*
     * Versteckte Triggerliste erstellen
     */
    hidden_trigger_fd = fopen(hidden_trigger_list, "r");
    if (hidden_trigger_fd != 0) {
	hidden_count = 0;
	while ((tmp =
		fgets(line, LINE_LEN, hidden_trigger_fd)) != (char *) 0) {
	    if ((i =
		 sscanf(line, "\"%[^\"] %*s \"%[^\"] %*s \"%[^\"] %*s %d ",
			name, gui, cmd, &time) == 4)) {
/***********
            set_trigger (name, time, gui, aflag, cmd, exp_mask, hidden)
***********/
		strcpy(hidden_trigger[hidden_count].name, name);
		strcpy(hidden_trigger[hidden_count].gui, gui);
		strcpy(hidden_trigger[hidden_count].cmd, cmd);
		hidden_trigger[hidden_count].time = time;
		hidden_trigger[hidden_count].aflag = 1;
		hidden_trigger[hidden_count].hidden = 1;
		hidden_trigger[hidden_count].experiment_mask = 0xffffffff;
		if (hidden_count == 0)
		    hidden_trigger_head = &hidden_trigger[hidden_count];
		else
		    hidden_trigger[hidden_count - 1].next =
			&hidden_trigger[hidden_count];
		hidden_trigger[hidden_count].next = (HIDDEN_LIST *) 0;
		hidden_count++;
	    }
	}
	fclose(hidden_trigger_fd);
	if (hidden_count)
	    swap_trigger_table = 1;
    }
/*******
   signal (SIGUSR1, configure);
*********/
}
