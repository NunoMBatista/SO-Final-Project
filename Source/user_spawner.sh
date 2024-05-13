#!/bin/bash

for i in {1..50}
do
   ./mobile_user 100 1000 500 200 400 23 &
done

# Wait for all background processes to finish
wait