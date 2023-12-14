#include "ManagementInterface.h"


#define MAXBUF 128
#define MANAGEMENT_PING_PORT 7355

void ManagementInterface::networkLoop(int sd) {
	unsigned int len;
	int n;
	char buffer_in[MAXBUF];
	struct sockaddr_in remote;
	struct timeval now;
	uint64_t sdif, usdif, tdif;

	len = sizeof(remote);

	while (1)
	{
		n = recvfrom(sd, buffer_in, MAXBUF, 0, (struct sockaddr*)&remote, &len);

		if (n >= 0) 
		{
			if (buffer_in[0] == 0xE5)
			{
				// Got ping request, respond:
				char responseBuffer[1] = { 0x12 };
				sendto(sd, &responseBuffer, sizeof(responseBuffer), 0, (struct sockaddr*)&remote, len);
			}
		}
	}
}


void* ManagementInterface::networkThreadEntry() {
	printf("Starting network thread in management class\n");

	sleep(1);

	// Setup socket
	int ld;
	struct sockaddr_in sockaddr;
	unsigned int length;
	struct ifreq ifr;


	if ((ld = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Problem creating socket\n");
		exit(1);
	}

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(MANAGEMENT_PING_PORT);

	if (bind(ld, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) 
	{
		printf("Problem binding\n");
		exit(0);
	}

	/*struct ifconf ifc;
	char buf[1024];
	int success = 0;

	uint32_t ip;
	uint32_t netmask;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(ld, SIOCGIFCONF, &ifc) == -1) 
	{
		printf("Problem ioctl SIOCGIFCONF\n");
		exit(1);
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	printf("Checking network interfaces ... \n");
	for (; it != end; ++it) 
	{
		strcpy(ifr.ifr_name, it->ifr_name);
		printf("%s: ", it->ifr_name);
		if (ioctl(ld, SIOCGIFFLAGS, &ifr) == 0) 
		{
			int not_loopback = !(ifr.ifr_flags & IFF_LOOPBACK);

			// Get IP
			ioctl(ld, SIOCGIFADDR, &ifr);
			ip = ntohl(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr);
			printf("inet %s ", inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));

			// Get netmask
			ioctl(ld, SIOCGIFNETMASK, &ifr);
			netmask = ntohl(((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr);
			printf("netmask %s\n", inet_ntoa(((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr));

			if (not_loopback) 
			{ 
				// Don't count loopback
				// Get MAC (EUI-48)
				if (ioctl(ld, SIOCGIFHWADDR, &ifr) == 0) {
					success = 1;
					break;
				}
			}
		}
		else {
			printf("Problem enumerate ioctl SIOCGIFHWADDR\n");
			exit(1);
		}
	}

	if (success) 
	{
		//
	}*/

	// Set Socket Timeout
	// This allows to react on cancel requests by control thread every 100us
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100;

	if (setsockopt(ld, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
		printf("setsockopt failed\n");

	if (setsockopt(ld, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
		printf("setsockopt failed\n");


	//Start Main Loop 
	networkLoop(ld);

	//Close Network Socket
	close(ld);


	return NULL;
}