# Galactica Server

## Supported Messages

```
srv <- join,nick -> join,id,_
srv <- ping -> pong
srv <- alive,id

srv <- usr-coord,id,x,y,angle
srv <- usr-fired,id,x,y,angle

srv -> del-ship,id
srv -> del-rock,id
srv -> del-pellet,id
srv -> del-bullet,id

srv -> cast,id,nick,event

srv -> stat-game,id,map-size,until-start,until-stop
srv -> stat-ship,id,x,y,angle,spice,energy,shield
srv -> stat-rock,id,x,y,speed,size,health
srv -> stat-bullet,id,pid,x,y,angle,time
srv -> stat-pellet,id,x,y,value,type
```
