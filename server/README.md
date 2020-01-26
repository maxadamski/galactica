# Galactica Server

## Supported Messages

```
srv <- join -> stat-join,id,_
srv <- ping -> pong
srv <- alive,id

srv <- ask-game,id   -> stat-game,id,_
srv <- ask-ship,id   -> {stat-ship,_}
srv <- ask-rock,id   -> {stat-rock,_}
srv <- ask-bullet,id -> {stat-bullet,_}
srv <- ask-pellet,id -> {stat-pellet,_}

srv <- hit-ship,id,oid   -> stat-ship,id,_
srv <- hit-rock,id,oid   -> stat-ship,id,_
srv <- hit-bullet,id,oid -> stat-ship,id,_
srv <- hit-pellet,id,oid -> stat-ship,id,_

srv <- shot-ship,id,oid -> stat-ship,oid,_ -> {stat-pellet,_}
srv <- shot-rock,id,oid -> stat-rock,oid,_ -> {stat-pellet,_}

srv <- usr-coord,id,x,y,angle
srv <- usr-fired,id,x,y,angle -> stat-bullet,_,id,_

srv -> del-ship,id
srv -> del-rock,id

srv -> stat-game,id,map-size,until-start,until-stop
srv -> stat-ship,id,x,y,angle,spice,energy,shield
srv -> stat-rock,id,x,y,speed,size,health
srv -> stat-bullet,id,pid,x,y,angle,time
srv -> stat-pellet,id,x,y,value,type
```
