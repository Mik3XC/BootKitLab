\ Usage:
\   cat code | gforth transl.fs -e '2 transl'

: .char ( c - ) dup 10 < if [char] 0 else 10 - [char] A then
		+ emit ;

: temit ( n - ) [char] c emit  dup   4 rshift .char   $0F and .char space ;

: process ( from to - ) ?do key temit loop ;

: transl ( to - ) 0 process cr bye ;
