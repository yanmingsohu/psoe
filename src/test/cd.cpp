#include "test.h"
#include "../cdrom.h"

namespace ps1e_t {


static void test_cdio() {
  char image[] = "ps-exe/Raiden Project, The (Europe).cue";
  ps1e::CdDrive cd;

  if (!cd.loadImage(image)) {
    return;
  }

  for (ps1e::CDTrack i = cd.first(); i < cd.end(); ++i) {
    printf("Track %d\n", i);
    ps1e::CdMsf msf;

    if (cd.getTrackMsf(i, &msf)) {
      printf("\tMM: %d, SS: %d, FF: %d\n", msf.m, msf.s, msf.f);
    }

    printf("\tFormat: %s\n", cd.trackFormatStr(cd.getTrackFormat(i)) );
  }
}


void test_cd() {
  //test_cdio();
}


}