export DOCKER_NAME=pintos
docker run --rm --name $DOCKER_NAME -v $(pwd):/pintos -e TERM=xterm-color -it n2sl2019/pintos:v1 bash