import os
import os.path
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

# Gmail full access scope for reading, searching, drafting & sending emails
SCOPES = ['https://mail.google.com/']

def main():
    creds = None
    token_path = '/Users/nolfonzo/code/SensorSentinel/token.json'
    cred_path = '/Users/nolfonzo/code/SensorSentinel/credentials.json'
    
    if os.path.exists(token_path):
        creds = Credentials.from_authorized_user_file(token_path, SCOPES)
        
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file(cred_path, SCOPES)
            creds = flow.run_local_server(port=0)
        with open(token_path, 'w') as token:
            token.write(creds.to_json())

    print("SUCCESSFULLY AUTHORIZED GMAIL API!")
    
    # Test reading mailbox profile
    service = build('gmail', 'v1', credentials=creds)
    profile = service.users().getProfile(userId='me').execute()
    print("GMAIL PROFILE AUTHORIZED:", profile.get('emailAddress'))
    print("TOTAL MESSAGES IN INBOX:", profile.get('messagesTotal'))

if __name__ == '__main__':
    main()
