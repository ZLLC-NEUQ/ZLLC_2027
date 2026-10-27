/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*
 * 这是调度程序使用的列表实现。  虽
 * 然它是专门针对调度程序的需求而定制
 * 的，但它也可供应用程序代码使用。
 *
 * list_ts只能存储指向list_item_ts的指针。
 * 每个 ListItem_t 包含一个数字 value (
 * xItemValue)。  大多数情况下，列表按项目值降序排序。
 *
 * 创建的列表已包含一个列表项。  该项目
 * 的值是可以存储的最大值，因此它始终位于
 * 列表的末尾并充当标记。  列表成员
 * pxHead 始终指向此标记 - 即使
 * 它位于列表的末尾。  这是因为尾部包含
 * 一个指向列表真正头部的回绕指针。
 *
 * 除了它的值之外，每个列表项还包含一个指向
 * list (pxNext) 中下一项的指针、一个
 * 指向列表 in (pxContainer) 的指
 * 针以及一个返回包含该列表的对象的指针。  包含后
 * 两个指针是为了提高列表操作的效率。  在包含列表
 * 项的对象和列表项本身之间实际上存在双向链接。
 *
 *
 * \page ListIntroduction 列表实施
 * \ingroup FreeRTOSIntro
 */

#ifndef INC_FREERTOS_H
	#error FreeRTOS.h must be included before list.h
#endif

#ifndef LIST_H
#define LIST_H

/*
 * 列表结构成员是在中断内修改的，因此按权利应声明为易
 * 失性的。  但是，它们仅在功能原子 way 中进行
 * 修改（在调度程序挂起的关键部分内），并且通过引用传
 * 递到函数中或通过易失性变量进行索引。  因此，在迄
 * 今为止测试的所有用例中，可以省略 易失性限定符，以
 * 便提供适度的性能改进，而不会对功能行为产生不利影响
 * 。  当各自的编译器选项设置为最大优化时，IAR、
 * ARM 和 GCC 编译器生成的汇编指令已经过检查
 * 并被视为符合预期。  也就是说，随着编译器技术的进
 * 步，特别是如果积极的跨模块优化是 used（尚未得
 * 到任何大程度运用的用例），那么需要
 * volatile 限定符来进行正确的优化是可行的。
 * 预计编译器会删除必要的代码，因为在列表结构成员
 * 上没有易失性限定符并且进行了积极的跨模块优化，编译
 * 器认为这些代码是不必要的，这将导致调度程序完全且明
 * 显的失败。  如果曾经遇到过这种情况，则只需将
 * configLIST_VOLATILE 定义为
 * FreeRTOSConfig.h 中的
 * volatile（按照此注释块底部的示例）即可将
 * 易失性限定符插入到列表结构中的相关位置。如果未定义
 * configLIST_VOLATILE，则下面的
 * 预处理器指令将完全#define configLIST_VOLATILE。
 *
 * 要使用易失性列表结构成员，请将以下行添加到
 * FreeRTOSConfig.h（不带引号）：“#
 * define configLIST_VOLATILE 易失性”
 */
#ifndef configLIST_VOLATILE
	#define configLIST_VOLATILE
#endif /* configSUPPORT_CROSS_MODULE_OPTIMISATION */

#ifdef __cplusplus
extern "C" {
#endif

/* 可用于将已知值放入列表结构中的宏，然后检
查已知值在应用程序执行期间不会被损坏。
这些可能会捕获内存中被覆盖的列表数
据结构。  它们不会捕获因
FreeRTOS 的错误配置或使用而导致的数据错误。*/
#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 )
	/* 定义宏不执行任何操作。 */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )
	#define listTEST_LIST_INTEGRITY( pxList )
#else
	/* 定义将新成员添加到列表结构中的宏。 */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue1;
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue2;
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue1;
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue2;

	/* 定义将新结构成员设置为已知值的宏。 */
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )		( pxItem )->xListItemIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )	( pxItem )->xListItemIntegrityValue2 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )		( pxList )->xListIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )		( pxList )->xListIntegrityValue2 = pdINTEGRITY_CHECK_VALUE

	/* 定义宏，如果结构成员之一不包
	含其预期值，则该宏将断言。 */
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )		configASSERT( ( ( pxItem )->xListItemIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxItem )->xListItemIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
	#define listTEST_LIST_INTEGRITY( pxList )			configASSERT( ( ( pxList )->xListIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxList )->xListIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
#endif /* configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES */


/*
 * 列表可以包含的唯一对象类型的定义。
 */
struct xLIST;
struct xLIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
	configLIST_VOLATILE TickType_t xItemValue;			/*< 列出的值。  在大多数情况下，这用于按降序对列表进行排序。 */
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;		/*< 指向列表中下一个 ListItem_t 的指针。 */
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;	/*< 指向列表中前一个 ListItem_t 的指针。 */
	void * pvOwner;										/*< 指向包含列表项的 object（通常是 TCB）的指针。  因此，包含列表项的对象和列表项本身之间存在双向链接。 */
	struct xLIST * configLIST_VOLATILE pxContainer;		/*< 指向该列表项为 placed（如果有）的列表的指针。 */
	listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
};
typedef struct xLIST_ITEM ListItem_t;					/* 由于某种原因，lint 希望将其作为两个单独的定义。 */

struct xMINI_LIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
	configLIST_VOLATILE TickType_t xItemValue;
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;

/*
 * 调度程序使用的队列类型的定义。
 */
typedef struct xLIST
{
	listFIRST_LIST_INTEGRITY_CHECK_VALUE				/*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
	volatile UBaseType_t uxNumberOfItems;
	ListItem_t * configLIST_VOLATILE pxIndex;			/*< 用于浏览列表。  指向调用 listGET_OWNER_OF_NEXT_ENTRY () 返回的最后一项。 */
	MiniListItem_t xListEnd;							/*< 包含最大可能项目值的列表项，这意味着它始终位于列表的末尾，因此用作标记。 */
	listSECOND_LIST_INTEGRITY_CHECK_VALUE				/*< 如果 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则设置为已知值。 */
} List_t;

/*
 * 访问宏以设置列表项的所有者。  列表项的所有者是
 * 包含该列表项的 object（通常是 TCB）。
 *
 * \页 listSET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )		( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/*
 * 访问宏以获取列表项的所有者。  列表项的所有者是
 * 包含该列表项的 object（通常是 TCB）。
 *
 * \页 listGET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )	( ( pxListItem )->pvOwner )

/*
 * 访问宏以设置列表项的值。  在大多数情
 * 况下，该值用于按降序对列表进行排序。
 *
 * \页 listSET_LIST_ITEM_VALUE listSET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )	( ( pxListItem )->xItemValue = ( xValue ) )

/*
 * 访问宏以检索列表项的值。  该值可
 * 以表示任何内容 - 例如任务的优先
 * 级，或者任务应解除阻塞的时间。
 *
 * \页 listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )	( ( pxListItem )->xItemValue )

/*
 * 访问宏以检索给定开头的列表项的值
 * 列表。
 *
 * \页 listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/*
 * 返回列表头部的列表项。
 *
 * \页 listGET_HEAD_ENTRY listGET_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext )

/*
 * 返回下一个列表项。
 *
 * \页 listGET_NEXT listGET_NEXT
 * \ingroup LinkedList
 */
#define listGET_NEXT( pxListItem )	( ( pxListItem )->pxNext )

/*
 * 返回标记列表末尾的列表项
 *
 * \页 listGET_END_MARKER listGET_END_MARKER
 * \ingroup LinkedList
 */
#define listGET_END_MARKER( pxList )	( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/*
 * 访问宏以确定列表是否包含任何项目。  仅
 * 当列表为空时，宏才会具有 true 值。
 *
 * \页 listLIST_IS_EMPTY listLIST_IS_EMPTY
 * \ingroup LinkedList
 */
#define listLIST_IS_EMPTY( pxList )	( ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) ? pdTRUE : pdFALSE )

/*
 * 访问宏以返回列表中的项目数。
 */
#define listCURRENT_LIST_LENGTH( pxList )	( ( pxList )->uxNumberOfItems )

/*
 * 获取列表中下一个条目的所有者的访问函数。
 *
 * 列表成员 pxIndex 用于遍历列表。  调用
 * listGET_OWNER_OF_NEXT_ENTRY
 * 将 pxIndex 递增到列表中的下一项，并返回
 * 该条目的 pxOwner 参数。  因此，通过多次
 * 调用此函数，可以移动列表中包含的每个项目。
 *
 * 列表项的 pxOwner 参数是指向拥有该
 * 列表项的对象的指针。  在调度程序中，这通
 * 常是一个任务控制块。 pxOwner 参数
 * 有效地在列表项及其所有者之间创建双向链接。
 *
 * @param pxTCB pxTCB 设置为下一个列表项的所有者的地址。
 * @param pxList 要从中返回下一个项目所有者的列表。
 *
 * \页 listGET_OWNER_OF_NEXT_ENTRY listGET_OWNER_OF_NEXT_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )										\
{																							\
List_t * const pxConstList = ( pxList );													\
	/* 将索引增加到下一个项目并返回该项目，确保 */				\
	/* 我们不返回列表末尾使用的标记。  */							\
	( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;							\
	if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )	\
	{																						\
		( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;						\
	}																						\
	( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;											\
}


/*
 * 用于获取列表中第一个条目的所有者的访问
 * 函数。  列表通常按项目值升序排序。
 *
 * 此函数返回列表中第一项的 pxOwner 成
 * 员。列表项的 pxOwner 参数是指向拥有
 * 该列表项的对象的指针。  在调度程序中，这通
 * 常是一个任务控制块。 pxOwner 参数有
 * 效地在列表项及其所有者之间创建双向链接。
 *
 * @param pxList 头项所有者所在的列表
 * 回来了。
 *
 * \页 listGET_OWNER_OF_HEAD_ENTRY listGET_OWNER_OF_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )

/*
 * 检查列表项是否在列表内。  列表项维护一
 * 个“容器”指针，该指针指向它所在的列表。
 * 该宏所做的只是检查容器和列表是否匹配。
 *
 * @param pxList 我们想知道列表项是否在其中的列表。
 * @param pxListItem 我们想知道是否在列表中的列表项。
 * 如果列表项在列表中，则为 @return pdTRUE，否则为 pdFALSE。
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( ( pxListItem )->pxContainer == ( pxList ) ) ? ( pdTRUE ) : ( pdFALSE ) )

/*
 * 返回包含 within（引用自）的列表项的列表。
 *
 * @param pxListItem 正在查询的列表项。
 * @return 指向引用 pxListItem 的 List_t 对象的指针
 */
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pxContainer )

/*
 * 这提供了了解列表是否已初始化的粗略方法，因为 pxList->
 * xListEnd.xItemValue 由 vListInitialise() 设置为 portMAX_DELAY
 * 功能。
 */
#define listLIST_IS_INITIALISED( pxList ) ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )

/*
 * 必须在使用列表之前调用！  这将初始化列
 * 表结构的所有成员，并将 xListEnd
 * 项插入列表中，作为列表后面的标记。
 *
 * @param pxList 指向正在初始化的列表的指针。
 *
 * \页 vListInitialise vListInitialise
 * \ingroup LinkedList
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/*
 * 必须在使用列表项之前调用。  这会将列表容器设置为
 * null，因此该项目不会认为它已包含在列表中。
 *
 * @param pxItem 指向正在初始化的列表项的指针。
 *
 * \页 vListInitialiseItem vListInitialiseItem
 * \ingroup LinkedList
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/*
 * 将列表项插入列表中。  该项目将被插入到列表中
 * 由其项目 value（项目值降序）确定的位置。
 *
 * @param pxList 要插入项目的列表。
 *
 * @param pxNewListItem 要放置在列表中的项目。
 *
 * \页 vListInsert vListInsert
 * \ingroup LinkedList
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 将列表项插入列表中。  该项目将被插入到一个位置，使
 * 其成为多次调用
 * listGET_OWNER_OF_NEXT_ENTRY 返回的列表中的最后一个项目。
 *
 * 列表成员 pxIndex 用于遍历列表。  调用
 * listGET_OWNER_OF_NEXT_ENTRY 会将
 * pxIndex 递增到列表中的下一项。使用
 * vListInsertEnd 将项目放置在列表中实际上是将项目放置在
 * pxIndex 指向的列表位置中。  这意味着在 pxIndex 参数
 * 再次指向要插入的项目之前，列表中的所有其他项目都将由 listGET_OWNER_OF_NEXT_ENTRY 返回。
 *
 * @param pxList 要插入项目的列表。
 *
 * @param pxNewListItem 要插入到列表中的列表项。
 *
 * \页 vListInsertEnd vListInsertEnd
 * \ingroup LinkedList
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * 从列表中删除一个项目。  列表项有一个指向其所
 * 在列表的指针，因此只需将列表项传递到函数中。
 *
 * @param uxListRemove 要删除的项目。  该项目将从中删除自身
 * 它的 pxContainer 参数指向的列表。
 *
 * @return 列表项删除后列表中剩余的项目数
 * 已被删除。
 *
 * \页 uxListRemove uxListRemove
 * \ingroup LinkedList
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif

