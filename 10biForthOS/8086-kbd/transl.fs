\ Usage:
\   cat code | gforth transl.fs -e '0 2 transl'

variable show 0 show !
: show? show @ if cr dup .  then ;

: .char ( c - ) dup 10 < if [char] 0 else 10 - [char] A then
		+ emit ;

: temit ( n - ) show?  [char] 1 emit  dup   4 rshift .char   $0F and .char ;

: ignore ( to - ) dup 0 ?do key drop loop ;
: process ( from to - ) ?do key temit loop ;

: transl ( from to - ) 2 + swap ignore process cr bye ;
