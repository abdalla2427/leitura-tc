clear;
%GET text data from ESP32 using HTTP
URLstring = "192.168.11.7/i_rms_data";
[S,SUCCESS] = urlread(URLstring);
if (SUCCESS)
  %process i_rms_data vector and sampleptr from text
  i_rms_data = strsplit(S,",");
  aux = strsplit (i_rms_data{1,size(i_rms_data,2)}," ");
  i_rms_data{1,size(i_rms_data,2)} = aux{1,1};
  sampleptr = str2num(aux{1,2});
  i_rms_data = cellfun(@str2num,i_rms_data);
  printf("Read %d samples from URL %s, with sample pointer at %d.\n",size(i_rms_data,2),URLstring,sampleptr);
  %rearrange cyclic vector considering sampleptr and vector size
  %sampleptr is zero-based and points to the next element in the vector
  %if ((sampleptr+1)<size(i_rms_data,2))
  %  i_rms_data = [i_rms_data((sampleptr+1) : size(i_rms_data,2)),i_rms_data(1 : (sampleptr+1)-1)];
  %endif
  %adjust time axis considering 0.5s between samples
  timeaxis = [0:(size(i_rms_data,2)-1)] * 0.5; 
  %plot i_rms_data with labels
  plot(timeaxis,i_rms_data);
  ylabel("i rms (A)");
  xlabel("t (s)");
  grid on;
  title("ESP32 Instantaneous RMS Current");
  ylim([0 8]);
  xlim([timeaxis(1) timeaxis(size(timeaxis,2))]);
else
  printf("URL %s was not read",URLstring)
endif