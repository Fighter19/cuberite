
// SslHTTPConnection.cpp

// Implements the cSslHTTPConnection class representing a HTTP connection made over a SSL link

#include "Globals.h"
#include "SslHTTPConnection.h"
#include "HTTPServer.h"





cSslHTTPConnection::cSslHTTPConnection(cHTTPServer & a_HTTPServer, const cX509CertPtr & a_Cert, const cCryptoKeyPtr & a_PrivateKey) :
	super(a_HTTPServer),
	m_Ssl(64000),
	m_Cert(a_Cert),
	m_PrivateKey(a_PrivateKey)
{
	m_Ssl.Initialize(false);
	m_Ssl.SetOwnCert(a_Cert, a_PrivateKey);
}





cSslHTTPConnection::~cSslHTTPConnection()
{
	m_Ssl.NotifyClose();
}





void cSslHTTPConnection::OnReceivedData(const char * a_Data, size_t a_Size)
{
	// Process the received data:
	const char * Data = a_Data;
	size_t Size = a_Size;
	for (;;)
	{
		// Try to write as many bytes into Ssl's "incoming" buffer as possible:
		size_t BytesWritten = 0;
		if (Size > 0)
		{
			BytesWritten = m_Ssl.WriteIncoming(Data, Size);
			Data += BytesWritten;
			Size -= BytesWritten;
		}

		// Try to read as many bytes from SSL's decryption as possible:
		char Buffer[32000];
		int NumRead = m_Ssl.ReadPlain(Buffer, sizeof(Buffer));
		if (NumRead > 0)
		{
			super::OnReceivedData(Buffer, static_cast<size_t>(NumRead));
			// The link may have closed while processing the data, bail out:
			return;
		}
		else if (NumRead == POLARSSL_ERR_NET_WANT_READ)
		{
			// SSL requires us to send data to peer first, do so by "sending" empty data:
			SendData(nullptr, 0);
		}

		// If both failed, bail out:
		if ((BytesWritten == 0) && (NumRead <= 0))
		{
			return;
		}
	}
}





void cSslHTTPConnection::SendData(const void * a_Data, size_t a_Size)
{
	const char * OutgoingData = reinterpret_cast<const char *>(a_Data);
	size_t pos = 0;
	for (;;)
	{
		// Write as many bytes from our buffer to SSL's encryption as possible:
		int NumWritten = 0;
		if (pos < a_Size)
		{
			NumWritten = m_Ssl.WritePlain(OutgoingData + pos, a_Size - pos);
			if (NumWritten > 0)
			{
				pos += static_cast<size_t>(NumWritten);
			}
		}

		// Read as many bytes from SSL's "outgoing" buffer as possible:
		char Buffer[32000];
		size_t NumBytes = m_Ssl.ReadOutgoing(Buffer, sizeof(Buffer));
		if (NumBytes > 0)
		{
			m_Link->Send(Buffer, NumBytes);
		}

		// If both failed, bail out:
		if ((NumWritten <= 0) && (NumBytes == 0))
		{
			return;
		}
	}
}




