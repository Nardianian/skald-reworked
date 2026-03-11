#!/bin/bash
# Setup script for GitHub PAT token
# Usage: ./setup-git-token.sh YOUR_TOKEN

if [ -z "$1" ]; then
    echo "❌ Error: No token provided"
    echo ""
    echo "Usage: ./setup-git-token.sh YOUR_GITHUB_TOKEN"
    echo ""
    echo "To get a GitHub token:"
    echo "1. Go to: https://github.com/settings/tokens"
    echo "2. Click 'Generate new token (classic)'"
    echo "3. Name: 'Skald Development'"
    echo "4. Select scope: 'repo'"
    echo "5. Click 'Generate token'"
    echo "6. Copy the token and run:"
    echo "   ./setup-git-token.sh ghp_YOUR_TOKEN_HERE"
    exit 1
fi

TOKEN="$1"

# Validate token format
if [[ ! $TOKEN =~ ^ghp_ ]]; then
    echo "❌ Error: Invalid token format"
    echo "GitHub Personal Access Tokens should start with 'ghp_'"
    exit 1
fi

echo "🔐 Setting up GitHub credentials..."

# Create .git directory if it doesn't exist
mkdir -p .git

# Write credentials file
echo "https://${TOKEN}@github.com" > .git/credentials

# Set permissions (make it readable only by you)
chmod 600 .git/credentials

echo "✅ Token saved to .git/credentials"
echo "✅ File permissions set to 600 (user read/write only)"
echo ""
echo "You can now use:"
echo "  ./git-push.sh   - Push to GitHub"
echo "  ./git-pull.sh   - Pull from GitHub"
echo "  ./git-sync.sh   - Pull and push in one command"
echo ""
echo "🚀 Ready to push Skald to GitHub!"