/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All rights reserved.
 *
 */
 
/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
/*
 * Module Name:    fsl_debug.h
 *
 * Description:    Head file for debug related macros.
 *
 * Portability:    This code is written for Linux OS.
 */  
 
/*
 * Changelog: 
 *
 */




#ifndef __FSL_DEBUG_H__
#define __FSL_DEBUG_H__
/* debug macros */
#define _STR(s)     #s
#define STR(s)      _STR(s)  

/* ANSI color print */
#define COLOR_RED       31
#define COLOR_GREEN     32
#define COLOR_YELLOW    33
#define COLOR_BLUE      34
#define COLOR_PURPLE    35
#define COLOR_CYAN      36

#define COLORFUL_STR(color, format, ...)\
    "\33[1;" STR(color) "m" format "\33[0m", ##__VA_ARGS__

#define YELLOW_STR(format,...)      COLORFUL_STR(COLOR_YELLOW,format, ##__VA_ARGS__)
#define RED_STR(format,...)         COLORFUL_STR(COLOR_RED,format, ##__VA_ARGS__)
#define GREEN_STR(format,...)       COLORFUL_STR(COLOR_GREEN,format, ##__VA_ARGS__)
#define BLUE_STR(format,...)        COLORFUL_STR(COLOR_BLUE,format,##__VA_ARGS__)
#define PURPLE_STR(format,...)      COLORFUL_STR(COLOR_PURPLE,format,##__VA_ARGS__)
#define CYAN_STR(format,...)        COLORFUL_STR(COLOR_CYAN,format,##__VA_ARGS__)


#define DEBUG_ERROR(format,...)  printf(RED_STR(format, ##__VA_ARGS__))
#define DEBUG_FLOW(format,...)  //printf(GREEN_STR(format, ##__VA_ARGS__))
#define DEBUG_MESSAGE(format,...) printf(BLUE_STR(format, ##__VA_ARGS__))

#endif
