/* 
 * Copyright (C) 1999-2001 Joachim Wieland <joe@mcknight.de>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111, USA.
 */

#include "jftpgw.h"
#include "cmds.h"

/* from std_cmds.c */
int std_quit(const char*, struct conn_info_st*);

int std_reset(const char*, struct conn_info_st*);
int std_user_split(const char*, struct conn_info_st*);
int std_user_plain(const char*, struct conn_info_st*);
int std_pass(const char*, struct conn_info_st*);
int fw_open(const char* args, struct conn_info_st* conn_info);
int fw_site(const char* args, struct conn_info_st* conn_info);
int fw_user(const char* args, struct conn_info_st* conn_info);
int fw_pass(const char* args, struct conn_info_st* conn_info);
int fw_fwpass(const char* args, struct conn_info_st* conn_info);
int fw_fwuser(const char* args, struct conn_info_st* conn_info);

int fw_login_type2(const char* args, struct conn_info_st* conn_info);

int fw_user_type7(const char* args, struct conn_info_st* conn_info);
int fw_pass_type7(const char* args, struct conn_info_st* conn_info);

int fw_user_type8(const char* args, struct conn_info_st* conn_info);

int fw_user_type9(const char* args, struct conn_info_st* conn_info);
int fw_pass_type9(const char* args, struct conn_info_st* conn_info);
int fw_acct_type9(const char* args, struct conn_info_st* conn_info);



#define RESETFUNC 0   /* first function in each handler-array */
#define QUITFUNC  1   /* second function in each handler-array */ 

/* The following firewall types are taken out of ncftp's configuration file.
 * See ftp://ftp.ncftp.com/ncftp by
 *
 * Mike Gleason
 * NcFTP Software
 * mgleason@NcFTP.com
 */

struct cmdhandlerstruct login_auth_funcs[][8] = {
	{
/* type 0:  Do NOT use a firewall                                         */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", std_user_plain },
		{ "PASS ", std_pass },
		{ 0, 0 }
	}, {
/* type 1:  Connect to firewall host, but send "USER user@real.host.name" */
/* "USER with no login (user@host port)"                                  */
/* "USER with no login (user@host:port)"                                  */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", std_user_split },
		{ "PASS ", fw_pass },
		{ 0, 0 },
	}, {
/* type 2:  Connect to firewall, login with "USER fwuser" and             */
/*          "PASS fwpassword", and then "USER user@real.host.name"        */
/* joe:     and thereafter "PASS password"                                */
/* "USER with login (user@host port)"                                  */
/* "USER with login (user@host:port)"                                  */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_fwuser },
		{ "PASS ", fw_fwpass },
		{ "USER ", std_user_split },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/* type 3:  Connect to and login to firewall, and then use                */
/*          "SITE real.host.name", followed by the regular USER and PASS. */
/* "SITE with login (user@host port)"                                  */
/* "SITE with login (user@host:port)"                                  */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_fwuser },
		{ "PASS ", fw_fwpass },

		{ "SITE ", fw_site },
		{ "USER ", fw_user },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/* type 4:  Connect to firewall, and then use (without login)             */
/*          "SITE real.host.name", followed by the regular USER and PASS. */
/*  SITE without login                                                    */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "SITE ", fw_site },
		{ "USER ", fw_user },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/* type 5:  Connect to and login to firewall, and then use                */
/*          "OPEN real.host.name", followed by the regular USER and PASS. */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_fwuser },
		{ "PASS ", fw_fwpass },

		{ "OPEN ", fw_open },
		{ "USER ", fw_user },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/* type 6:  Connect to firewall, and then use (without login)             */
/*          "OPEN real.host.name", followed by the regular USER and PASS. */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "OPEN ", fw_open },
		{ "USER ", fw_user },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/* type 7:  Connect to firewall host, but send                            */
/*           "USER user@fwuser@real.host.name" and                        */
/*           "PASS pass@fwpass" to login.                                 */
/* CheckPoint Firewall 1                                                  */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_user_type7 },
		{ "PASS ", fw_pass_type7 },
		{ 0, 0 }
	}, {
/*  type 8:  Connect to firewall host, but send                           */
/*           "USER fwuser@real.host.name" and                             */
/*           "PASS fwpass" followed by a regular                          */
/*           "USER user" and                                              */
/*           "PASS pass" to complete the login.                           */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_user_type8 },
		{ "PASS ", fw_fwpass },
		{ "USER ", fw_user },
		{ "PASS ", fw_pass },
		{ 0, 0 }
	}, {
/*  type 9:  Connect to firewall host, but send                           */
/*           "USER user@real.host.name fwuser" and                        */
/*           "PASS pass" followed by                                      */
/*           "ACCT fwpass" to complete the login.                         */
/* User@host FireID                                                       */
		{ "reset", std_reset },
		{ "QUIT", std_quit },

		{ "USER ", fw_user_type9 },
		{ "PASS ", fw_pass_type9 },
		{ "ACCT ", fw_acct_type9 },
		{ 0, 0 }
	}
};

