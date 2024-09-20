#!/bin/bash

for i in {1..50}
do
   ./mobile_user 100 1000 1000 2000 4000 23 &
done

# Wait for all background processes to finish
wait