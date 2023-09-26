## 코치님 질문에 대한 답안을 적어보자..

1. %rip, %rsp register의 의미와 CPU 동작에서의 역할
%rip는 현재 인스트럭션을 가리킨다. %rsp는 스택의 top을 가리킨다.
2. assembly 명령어 JMP와 CALL의 차이점
JMP는 특정 주소의 인스트럭션으로 이동한다. CALL은 다음에 실행되어야 할 인스트럭션 주소를 stack에 저장해두고 이동한다. stack에 어떻게 저장하는가? %rsp를 8 감소시켜서 스택 영역을 8byte 넓히고, movq 주소 %rsp 를 한다.
3. JMP, CALL, RET, PUSH, POP, IRET 명령에서 %rip, %rsp는 어떻게 값이 변하는가?
* JMP 0xa를 하면 %rip는 0xa가 된다. %rsp는 변하지 않는다.
* CALL 0xa을 하면 %rip는 0xa가 된다. %rsp는 8 감소한다. (다음에 실행될 인스트럭션 주소 크기만큼)
* RET을 하면 %rip는 (%rsp) 가 된다? %rsp는 8 증가한다.
* PUSH a를 하면 %rsp는 a의 크기(8)만큼 감소한다. %rip는 변하지 않는다.
* POP을 하면 %rsp는 8만큼 증가시킨다. %rip는 변하지 않는다.
* IRET을 하면 %rsp는 8만큼 증가한다. %rip는 .. 인터럽트 처리 결과에 따라 정해진다.
4. interrupt 발생 시 CPU는 어떻게 동작하는가? register 값 및 연관된 memory는?
interrupt 발생하면 CPU는 해당 인터럽트 벡터값에 매핑된 interrupt handler를 idt에서 찾아 call한다. call을 하면 interrupt handler가 iret으로 반환된 이후 실행되어야 할 인스트럭션의 주소를 스택에 저장하고, interrupt handler의 주소로 %rip를 변경한다.
5. interrupt를 처리한 후에는 어떻게 되는가?
inpterrupt처리가 완료된 후에는, call전에 stack에 push해두었던 주소를 찾아 실행할 수도 있고, 또는 다른 인스트럭션으로 jump할수도 있다.
6. interrupt는 어떻게 enable/disable 시킬 수 있는가? enable/disable 되면 뭐가 달라지는가?
interrupt flag를 끄면 disable, 켜면 enable이다. interrupt flag는 EFLAG 레지스터의 9번째 비트에 위치하고 있어, 1인 경우 인터럽트를 허용, 0인 경우 인터럽트를 비허용한다. 어셈블리 명령어로는 sti (set interrupt flag)가 인터럽트를 허용하고, cli (clear interrupt flag)가 인터럽트를 비허용한다. sti는 인터럽트를 허용하지만, 바로 다음에 나오는 명령어까지는 인터럽트를 비허용한다. 즉, sti; hlt;를 하면 이들 명령어가 다 실행될 때까지는 인터럽트에 의해 방해받지 않고 아토믹하게 실행된다는 것이다.
7. Option: NMI (Non-Maskable Interrupt)는 무엇인가?
인터럽트 마스킹이란, 핸들링 중인 인터럽트를 중단하고 발생한 인터럽트를 먼저 처리하는 것이다. Non Maskable Interrupt란 이렇게 중단될 수 없는 인터럽트를 말한다.
8. timer interrupt는 누가 발생시키는가?
8254 Timer (Programmable Timer)가 있다. 이 타이머를 timer_init등으로 일정 간격마다 인터럽트를 발생시키도록 설정하면, 벡터값 0x20을 간격마다 CPU내 IPC (Interrupt ? Controler)칩으로 보내서 인터럽트를 발생시킨다.
9. EFLAGS register는 무엇인가?
상태를 나타내는 정보를 담고 있는 레지스터이다. Interrupt 허용/비허용을 설정하는 Interrupt Flag (IF)가 이 레지스터에 있다.
10. Option: "플래그 세우지 마라"의 그 flag인가?
11. Option: 왜 다른 64bit register처럼 r로 시작하지 않고 e로 시작하는가?