#!/bin/bash
wget https://ajax.googleapis.com/ajax/libs/angularjs/1.3.8/angular.min.js
wget https://raw.githubusercontent.com/cornflourblue/angu-fixed-header-table/master/angu-fixed-header-table.js
wget https://raw.githubusercontent.com/danielstern/ngAudio/master/app/angular.audio.js
wget https://maxcdn.bootstrapcdn.com/bootstrap/3.3.1/css/bootstrap.min.css
wget https://raw.githubusercontent.com/adamalbrecht/ngModal/master/dist/ng-modal.min.js
wget https://raw.githubusercontent.com/adamalbrecht/ngModal/master/dist/ng-modal.css
fa=font-awesome-4.3.0
wget https://fortawesome.github.io/Font-Awesome/assets/$fa.zip
unzip $fa.zip
rm $fa.zip
ln -s $fa font-awesome
