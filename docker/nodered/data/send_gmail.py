import sys, os, base64
from google.oauth2.credentials import Credentials
from google.auth.transport.requests import Request
from googleapiclient.discovery import build
from email.message import EmailMessage

def send_message(subject, body, to="nolfonzo@gmail.com"):
    token_path = os.path.join(os.path.dirname(__file__), 'token.json')
    if not os.path.exists(token_path):
        print("ERROR: token.json not found")
        sys.exit(1)
        
    creds = Credentials.from_authorized_user_file(token_path)
    if creds and creds.expired and creds.refresh_token:
        creds.refresh(Request())
        
    service = build('gmail', 'v1', credentials=creds)
    message = EmailMessage()
    message.set_content(body)
    message['To'] = to
    message['From'] = 'nolfonzo@gmail.com'
    message['Subject'] = subject

    encoded_message = base64.urlsafe_b64encode(message.as_bytes()).decode()
    create_message = {'raw': encoded_message}

    sent = service.users().messages().send(userId="me", body=create_message).execute()
    print("GMAIL SENT OK! Message ID:", sent['id'])

if __name__ == '__main__':
    subj = sys.argv[1] if len(sys.argv) > 1 else "SensorSentinel Test"
    body = sys.argv[2] if len(sys.argv) > 2 else "Test body"
    to_email = sys.argv[3] if len(sys.argv) > 3 else "nolfonzo@gmail.com"
    send_message(subj, body, to_email)
