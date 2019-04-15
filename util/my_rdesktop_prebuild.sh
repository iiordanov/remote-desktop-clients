#!/bin/sh

export PROJECT=aRDP
./eclipse_projects/download-prebuilt-dependencies.sh 
./eclipse_projects/bVNC/copy_prebuilt_files.sh $PROJECT
./eclipse_projects/bVNC/prepare_project.sh --skip-build $PROJECT nopath nopath

