
#Add the remote origin
git remote add upstream https://gitlab.com/petsc/petsc.git

# Pull the remote main branch
git pull upstream main


# Fix any conflicts

git commit -a -m "Updating PETSc to main branch level"


git push 

cd ../

git add petsc

git commit -a -m "Updated Petsc to latest commit"

git push




