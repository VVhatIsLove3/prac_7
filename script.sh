#!/bin/bash

# Создаем директории и файлы
mkdir -p $HOME/instance/t
cd $HOME/instance

# Создаем и заполняем файлы 1-5
words=("the THE HELLO ThE tHE hello qq QQ Qq qQ" 
       "the THE ABBA abba ABBa AbbA" 
       "the THE Esc ESC esC esc" 
       "the THE CTRL ctrl CTrl CTRl" 
       "the THE TAB Tab TAb TaB")
for i in {1..5}; do
    touch $i
    for word in ${words[i-1]}; do
        echo $word >> $i
    done
done
# Создаем и заполняем скрытый файл .6
touch .6
for word in the THE TAB Tab TAb esc ctrl abba qqs; do
    echo $word >> .6
done
# Работаем в поддиректории t
cd t
touch 7 .8
for word in the THE TAB the esc hello abba; do
    echo $word >> 7
    echo $word >> .8
done
cd ..
mkdir .j
cd .j
touch .l h
echo the >> .l
echo tab >> h
