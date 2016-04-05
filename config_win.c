#include <pcap.h>

#include <stdio.h>

int main(int argc, char **argv)
{
	pcap_if_t *alldevs, *d;
	char errbuf[PCAP_ERRBUF_SIZE];
	int i = 0;

	printf("Get all interface...\n");
	if (-1 == pcap_findalldevs(&alldevs, errbuf)) {
		fprintf(stderr, "Get interface handler:%s\n", errbuf);
		return 1;
	}
	printf("There are all interface:\n");
	for (d = alldevs; NULL != d; d = d->next)
		printf("%d. %s (%s)\n", ++i, d->name, d->description);

	pcap_freealldevs(alldevs);
	return 0;
}


