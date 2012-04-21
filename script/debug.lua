
function info()
  puts(
    string.format(
      'name:%s state:%d>%d %s move:%s physics:%s',
      name(), prevstateno(), stateno(), statetype(), movetype(), physics()))
  puts(
    string.format(
      'anim:%d %d elem:%d %d pos:%.3f,%.3f vel:%.3f,%.3f',
      anim(), animtime(), animelemno(0), animelemtime(animelemno(0)),
      posX(), posY(), velX(), velY()))
end

