% This code is based on an example provided by Giulio Moro, April 20 2015.

Fs=44100;
t = 1/Fs;
t2 = t*t;
q = sqrt(2); % q is sqrt(2) for 10 bands and 10 bands is all I can handle.

f=[22, 44, 88, 177, 355, 710, 1420, 2840, 5680, 11360];
geoMeans(1)=sqrt(22*44);
geoMeans(2)=sqrt(44*88);
geoMeans(3)=sqrt(88*177);
geoMeans(4)=sqrt(177*355);
geoMeans(5)=sqrt(710*355);
geoMeans(6)=sqrt(710*1420);
geoMeans(7)=sqrt(1420*2840);
geoMeans(8)=sqrt(2840*5680);
geoMeans(9)=sqrt(5680*11360);
geoMeans(10)=sqrt(11360*22720);
clear co
i=1;
  [B,A]=butter(2,f(i)/22050,'low');
  co(i,1:2)=A(2:3);
  co(i,3:5)=B(:);

for i=2:9
  [B,A]=butter(1,[f(i:i+1)]/22050,'bandpass');
  co(i,1:2)=A(2:3);
  co(i,3:5)=B(:);
end

i=10;
  [B,A]=butter(2,f(i)/22050,'high');
  co(i,1:2)=A(2:3);
  co(i,3:5)=B(:);
 

co=[ones(10,1) co];
F=co;
figure(2)
clf
hold on
x=[1;zeros(1023,1)];
for i=1:10
  x=20*log10(abs(freqz(F(i,4:6),F(i,1:3))));
  plot((0:511)*22050/512,x);
end
axis([20 20000 -60 0]);
set(gca,'xscale','log')
title('coefficients from Matlab')
grid on
print('-dpng','matlabCoeff.png')
hold off
  
  
F=[-1.995572, 0.995577, 0.000002, 0.000005, 0.000002
-1.993672, 0.993751, 0.003125, 0.000000, -0.003125
-1.987191, 0.987506, 0.006247, 0.000000, -0.006247
-1.973806, 0.975066, 0.012467, 0.000000, -0.012467
-1.945715, 0.950705, 0.024648, 0.000000, -0.024648
-1.884519, 0.903986, 0.048007, 0.000000, -0.048007
-1.744093, 0.818264, 0.090868, 0.000000, -0.090868
-1.406099, 0.676932, 0.161534, 0.000000, -0.161534
-0.572807, 0.507133, 0.246433, 0.000000, -0.246433
0.055924, 0.172126, 0.279051, -0.558101, 0.279051
];
F=[ones(10,1) F];
figure(1)
clf
hold on
x=[1;zeros(1023,1)];
for i=1:10
  x=20*log10(abs(freqz(F(i,4:6),F(i,1:3))));
  plot((0:511)/512*22050,x);
end
axis([20 20000 -60 0]);
set(gca,'xscale','log')
grid on
hold off
title('coefficients from C')
print('-dpng','astridCoeff.png')