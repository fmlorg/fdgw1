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

int std_loggedin(const char*, struct conn_info_st*);
int std_quit(const char*, struct conn_info_st*);
int std_pasv(const char*, struct conn_info_st*);
int std_epsv(const char*, struct conn_info_st*);
int std_port(const char*, struct conn_info_st*);
int std_stor(const char*, struct conn_info_st*);
int std_retr(const char*, struct conn_info_st*);
int std_type(const char*, struct conn_info_st*);
int std_list(const char*, struct conn_info_st*);


struct cmdhandlerstruct std_cmdhandler[] = {
	{ "USER ", std_loggedin },
	{ "PASS ", std_loggedin },
	{ "PORT ", std_port },
	{ "PASV",  std_pasv },
	{ "EPSV",  std_epsv },
	{ "STOR ", std_stor },
	{ "RETR ", std_retr },
	{ "APPE ", std_stor },
	{ "TYPE ", std_type },
	{ "QUIT", std_quit },
	{ "LIST", std_list },
	{ "NLST", std_list },
	{ 0, 0 }
};



