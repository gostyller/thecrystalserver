#!/bin/bash

#Crystal server run script

echo "Crystal server run script"
VERSION="1.00a"

case "$1" in
  --help|-h)
      echo >&2 "Usage $0 <options>"
      echo >&2 " Where options are:"
      echo >&2 " -h|--help|start|stop|restart|kill|--version";
      if [ -f crystalserver ];
      then
        ./crystalserver --help
      fi
      ;;
   start|s)
     if [ -f crystalserver ];
     then
        echo >&2 "Running The forgotten server v$VERSION"
        ./crystalserver
        echo >&2 "Exited with code $?"
      else
         echo >&2 "Could not start The forgotten server"
      fi
     ;;
   restart)
     echo "Restarting..."
     killall -9 crystalserver
     if [ -f crystalserver ];
     then
        ./crystalserver
		echo >&2 "Exited with code $?"
     else
        echo >&2 "Failed"
     fi
     ;;
   stop|kill)
     killall -9 crystalserver
     ;;
   --version)
     echo "v$VERSION"
     exit
     ;;
   *)
      echo >&2 "Usage $0 <options>"
      echo >&2 " Where options are:"
      echo >&2 " -h|--help|start|stop|restart|kill";
      ;;
esac
