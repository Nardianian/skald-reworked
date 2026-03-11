#!/bin/bash
# Helper script to pull Skald using PAT token
# Usage: ./git-pull.sh [branch]
# Default branch: main

BRANCH="${1:-main}"
REPO_URL="github.com/josephvolmer/skald.git"

# Read token from .git/credentials
TOKEN=$(cat .git/credentials 2>/dev/null | grep -o 'ghp_[^@]*')

if [ -z "$TOKEN" ]; then
    echo "❌ Error: No GitHub PAT token found in .git/credentials"
    echo "Please create .git/credentials with format:"
    echo "https://ghp_YOUR_TOKEN@github.com"
    exit 1
fi

echo "🔐 Configuring git remote with PAT..."

# Temporarily set remote URL with token
git remote set-url origin "https://${TOKEN}@${REPO_URL}"

echo "⬇️  Pulling from origin/${BRANCH}..."

# Pull
git pull origin "$BRANCH"
EXIT_CODE=$?

# Clean up: restore remote URL without token
echo "🧹 Cleaning up remote URL..."
git remote set-url origin "https://${REPO_URL}"

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ Successfully pulled from ${BRANCH}!"
else
    echo "❌ Pull failed with exit code ${EXIT_CODE}"
fi

exit $EXIT_CODE
